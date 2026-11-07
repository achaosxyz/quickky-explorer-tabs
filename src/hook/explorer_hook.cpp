#include "common/hook_protocol.h"
#include "explorer/native_tab_guard.h"
#include "explorer/folder_location.h"
#include "explorer/native_frame.h"
#include "chrome/tab_strip.h"

#include <commctrl.h>
#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

namespace {

constexpr UINT_PTR kSubclass = 0x514b4750;
constexpr GUID kEmptyNonce{};
constexpr wchar_t kBindingProperty[] = L"QuickkyExplorerTabs.Attachment";
using ProcessHandle = std::unique_ptr<void, decltype(&CloseHandle)>;

// One attachment per Explorer UI thread. The window property binds its HWND
// lifetime; never restore a reused handle. Commit defers teardown while creating
// chrome; root/child callback keepalives protect every later reentrant operation.
struct Attachment {
    ~Attachment()
    {
        if (exchange) UnmapViewOfFile(exchange);
        if (target && GetPropW(target, kBindingProperty) == this) RemovePropW(target, kBindingProperty);
    }
    HWND target = nullptr;
    HWND sender = nullptr;
    GUID nonce{};
    ProcessHandle controller{nullptr, CloseHandle};
    ProcessHandle lease{nullptr, CloseHandle};
    quickky::GuardRequest identity;
    WINDOWPLACEMENT original{sizeof(WINDOWPLACEMENT)};
    bool original_visible = true;
    bool changed = false;
    bool committed = false;
    bool layout_pending = false;
    bool initializing = false;
    bool stop_requested = false;
    bool restore_allowed = true;
    HHOOK keyboard = nullptr;
    quickky::TabStrip strip;
    UINT_PTR timer = 0;
    quickky::NativeTabGuard guard;
    std::shared_ptr<quickky::NativeFrame> native;
    ProcessHandle mapping{nullptr, CloseHandle};
    ProcessHandle completed{nullptr, CloseHandle};
    quickky::NativeExchange* exchange = nullptr;
    std::unique_ptr<quickky::NativeExchange> pending;
    quickky::GuardCommand pending_command = quickky::GuardCommand::NativeSnapshot;
    std::uint64_t sequence = 0;
};

thread_local std::shared_ptr<Attachment> attachment;
thread_local UINT native_message = 0;
thread_local UINT layout_message = 0;
LRESULT CALLBACK RootProcedure(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
LRESULT CALLBACK Keyboard(int, WPARAM, LPARAM);

bool Decode(LPARAM parameter, quickky::GuardRequest& request)
{
    const auto data = reinterpret_cast<const COPYDATASTRUCT*>(parameter);
    if (!data || data->dwData != quickky::kGuardProtocol ||
        data->cbData != sizeof(request) || !data->lpData) return false;
    std::memcpy(&request, data->lpData, sizeof(request));
    return request.size == sizeof(request) && request.version == quickky::kGuardVersion &&
        request.count <= quickky::kMaxWorkspaceTabs;
}

void Detach(bool restore = true)
{
    if (attachment && attachment->initializing) {
        attachment->stop_requested = true;
        attachment->restore_allowed = attachment->restore_allowed && restore;
        attachment->guard.Stop();
        attachment->strip.Hide();
        if (attachment->native) attachment->native->StopPlus();
        return;
    }
    auto old = std::move(attachment);
    if (!old) return;
    // Clear thread ownership before Win32 teardown can reenter our callbacks.
    if (old->timer) KillTimer(nullptr, old->timer);
    if (old->keyboard) UnhookWindowsHookEx(old->keyboard);
    old->guard.Stop();
    old->strip.Hide();
    if (old->native) old->native->StopPlus();
    if (GetPropW(old->target, kBindingProperty) == old.get()) {
        RemoveWindowSubclass(old->target, RootProcedure, kSubclass);
    }
    if (restore && old->changed && GetPropW(old->target, kBindingProperty) == old.get()) {
        auto placement = old->original;
        if (!old->original_visible) placement.showCmd = SW_HIDE;
        SetWindowPlacement(old->target, &placement);
    }
}

bool ControllerAvailable()
{
    return attachment && !attachment->stop_requested &&
        WaitForSingleObject(attachment->controller.get(), 0) == WAIT_TIMEOUT &&
        (!attachment->lease || WaitForSingleObject(attachment->lease.get(), 0) == WAIT_TIMEOUT);
}

void CALLBACK CheckController(HWND, UINT, UINT_PTR timer, DWORD)
{
    if (attachment && attachment->timer == timer &&
        (!ControllerAvailable() ||
         (attachment->identity.workspace != 2 &&
          (quickky::NativeTabGuard::CountTabs(attachment->target) != 1 ||
           (attachment->identity.workspace != 0 && attachment->committed && !attachment->strip.alive()))))) Detach();
}

bool SendInput(quickky::TabInput action, std::uint64_t tab)
{
    try {
        if (!attachment || !attachment->lease || !attachment->committed || !ControllerAvailable()) return false;
        const auto lifetime = attachment;
        auto request = lifetime->identity;
        request.command = quickky::GuardCommand::Input;
        request.input = action;
        request.input_tab = tab;
        COPYDATASTRUCT data{quickky::kGuardProtocol, sizeof(request), &request};
        std::vector<unsigned char> snapshot;
        if (action == quickky::TabInput::New) {
            const auto folder = quickky::ResolveFolder(lifetime->target);
            if (!folder || attachment != lifetime || !ControllerAvailable()) return false;
            const auto bytes = ILGetSize(folder->pidl.get());
            if (!bytes || bytes > quickky::kMaxFolderSnapshotBytes) return false;
            snapshot.resize(sizeof(request) + bytes);
            request.size = static_cast<std::uint32_t>(snapshot.size());
            std::memcpy(snapshot.data(), &request, sizeof(request));
            std::memcpy(snapshot.data() + sizeof(request), folder->pidl.get(), bytes);
            data.cbData = request.size;
            data.lpData = snapshot.data();
        }
        DWORD_PTR result = 0;
        return SendMessageTimeoutW(attachment->sender, WM_COPYDATA,
            reinterpret_cast<WPARAM>(attachment->target), reinterpret_cast<LPARAM>(&data),
            SMTO_ABORTIFHUNG | SMTO_BLOCK, 500, &result) != 0 && result == 1;
    } catch (...) { return false; } // No C++ exception may cross a Win32 hook callback.
}

LRESULT CALLBACK Keyboard(int code, WPARAM wparam, LPARAM lparam)
{
    try {
    if (code >= 0 && wparam == PM_REMOVE && attachment && attachment->lease &&
        attachment->committed && ControllerAvailable()) {
        auto message = reinterpret_cast<MSG*>(lparam);
        if (message && message->message == WM_KEYDOWN &&
            GetAncestor(message->hwnd, GA_ROOT) == attachment->target &&
            (GetKeyState(VK_CONTROL) & 0x8000)) {
            bool handled = true;
            if (attachment->identity.workspace == 2) {
                handled = false;
                if (message->wParam == 'T' && !(GetKeyState(VK_SHIFT) & 0x8000)) {
                    const auto lifetime = attachment;
                    quickky::FolderLocation folder;
                    std::vector<GUID> tabs;
                    GUID active{};
                    GUID created{};
                    handled = lifetime->native && lifetime->native->Snapshot(tabs, active, folder) &&
                        attachment == lifetime && ControllerAvailable() &&
                        SUCCEEDED(lifetime->native->Add(folder.pidl.get(), created));
                    // A failed selection can still leave one created tab. Never
                    // let Explorer repeat Ctrl+T after that partial native add.
                    handled = handled || !InlineIsEqualGUID(created, GUID{});
                }
                if (handled) message->message = WM_NULL;
                return CallNextHookEx(nullptr, code, wparam, lparam);
            }
            if (message->wParam == 'T' && !(GetKeyState(VK_SHIFT) & 0x8000)) {
                handled = SendInput(quickky::TabInput::New, 0);
            } else if (message->wParam == 'W') {
                handled = SendInput(quickky::TabInput::Close, 0);
            } else if (message->wParam == VK_TAB) {
                handled = SendInput((GetKeyState(VK_SHIFT) & 0x8000) ? quickky::TabInput::Previous :
                    quickky::TabInput::Next, 0);
            } else handled = false;
            if (handled) message->message = WM_NULL;
        }
    }
    } catch (...) { /* Leave unhandled input to Explorer; never unwind a hook. */ }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

bool Commit()
{
    if (!attachment || !attachment->lease) return false;
    if (attachment->committed) return true;
    auto* current = attachment.get();
    if (current->initializing) return false;
    current->initializing = true;
    bool initialized = false;
    try {
        if (current->identity.workspace == 2) {
            current->native = quickky::NativeFrame::Bind(current->target);
            initialized = current->native && current->native->InstallPlus(attachment, [current] {
                return attachment.get() == current && current->committed && ControllerAvailable();
            });
        } else {
            initialized = GetWindowPlacement(current->target, &current->original) &&
                current->guard.Start(current->target);
            current->original_visible = IsWindowVisible(current->target) != FALSE;
            if (initialized && current->identity.workspace == 3) {
                initialized = current->guard.SuppressBar(attachment, [current] {
                    if (attachment.get() == current) Detach();
                });
            }
            if (initialized) initialized = current->strip.Create(current->target, SendInput, attachment,
                current->identity.workspace == 1);
        }
        if (initialized && !current->stop_requested) {
            current->keyboard = SetWindowsHookExW(WH_GETMESSAGE, Keyboard, nullptr, GetCurrentThreadId());
            initialized = current->keyboard != nullptr;
        }
    } catch (...) {
        current->initializing = false;
        Detach(current->restore_allowed);
        return false;
    }
    current->initializing = false;
    if (!initialized || !ControllerAvailable() ||
        GetPropW(current->target, kBindingProperty) != current) {
        Detach(current->restore_allowed);
        return false;
    }
    current->committed = true;
    return true;
}

bool Matches(HWND target, WPARAM sender, const quickky::GuardRequest& request)
{
    return attachment && attachment->target == target &&
        GetPropW(target, kBindingProperty) == attachment.get() &&
        attachment->sender == reinterpret_cast<HWND>(sender) &&
        request.target == reinterpret_cast<std::uint64_t>(target) &&
        InlineIsEqualGUID(request.nonce, attachment->nonce);
}

LRESULT QueueNativeCommand(const quickky::GuardRequest& request)
{
    const auto lifetime = attachment;
    if (!lifetime || !lifetime->committed || !lifetime->native || !lifetime->exchange || lifetime->pending ||
        (request.command != quickky::GuardCommand::NativeSnapshot &&
         request.command != quickky::GuardCommand::NativeAdd && request.command != quickky::GuardCommand::NativeRemove)) return 0;
    auto shared = lifetime->exchange;
    if (shared->version != quickky::kGuardVersion || shared->request != request.input_tab ||
        shared->request <= lifetime->sequence || InterlockedCompareExchange(&shared->state, 1, 0) != 0) return 0;
    lifetime->pending = std::make_unique<quickky::NativeExchange>();
    std::memcpy(lifetime->pending.get(), shared, sizeof(quickky::NativeExchange));
    lifetime->sequence = lifetime->pending->request;
    lifetime->pending_command = request.command;
    // Native navigation may call COM. Execute from a posted UI message rather
    // than an input-synchronous WM_COPYDATA stack, where such callouts can fail.
    return PostMessageW(lifetime->target, native_message, 0, 0) ? 1 : 0;
}

void ExecuteNativeCommand()
{
    const auto lifetime = attachment;
    if (!lifetime || !lifetime->pending || !lifetime->native || !ControllerAvailable()) return;
    auto owned_packet = std::move(lifetime->pending);
    auto& packet = *owned_packet;
    const auto command = lifetime->pending_command;
    auto shared = lifetime->exchange;
    packet.result = E_ABORT;
    auto folder = quickky::CloneFolderSnapshot(packet.folder, packet.folder_bytes);
    if (command == quickky::GuardCommand::NativeSnapshot) {
        std::vector<GUID> tabs;
        GUID active{};
        quickky::FolderLocation location;
        if (lifetime->native->Snapshot(tabs, active, location) && attachment == lifetime && ControllerAvailable()) {
            const auto size = ILGetSize(location.pidl.get());
            if (size && size <= sizeof(packet.folder) && tabs.size() <= quickky::kMaxNativeTabs) {
                packet.count = static_cast<std::uint32_t>(tabs.size());
                std::copy(tabs.begin(), tabs.end(), packet.tabs);
                packet.active = active;
                packet.folder_bytes = size;
                std::memcpy(packet.folder, location.pidl.get(), size);
                packet.result = S_OK;
            }
        }
    } else if (folder && attachment == lifetime && ControllerAvailable()) {
        if (command == quickky::GuardCommand::NativeAdd) {
            packet.result = lifetime->native->Add(folder.get(), packet.created);
        } else if (command == quickky::GuardCommand::NativeRemove) {
            packet.result = lifetime->native->Remove(packet.active, folder.get());
        }
    }
    // Removing the last source tab can destroy its root and detach reentrantly.
    // The local keepalive owns this mapping until the acknowledgment is complete.
    packet.state = 1;
    std::memcpy(shared, &packet, sizeof(packet));
    InterlockedExchange(&shared->state, 2);
    SetEvent(lifetime->completed.get());
}

LRESULT HandleRootMessage(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (native_message && message == native_message) { ExecuteNativeCommand(); return 0; }
    if (layout_message && message == layout_message) {
        const auto current = attachment;
        if (current && current->target == window && current->identity.workspace == 1 &&
            current->committed && current->layout_pending) {
            current->layout_pending = false;
            if (!ControllerAvailable() || (!current->strip.Resize() && attachment == current)) Detach();
        }
        return 0;
    }
    if (message == WM_COPYDATA) {
        quickky::GuardRequest request;
        if (Decode(lparam, request) && Matches(window, wparam, request)) {
            if (request.command == quickky::GuardCommand::Detach) { Detach(); return 1; }
            if (!ControllerAvailable()) { Detach(); return 0; }
            if (attachment->identity.workspace != 2 && attachment->lease && attachment->committed &&
                (quickky::NativeTabGuard::CountTabs(window) != 1 || !attachment->strip.alive())) { Detach(); return 0; }
            if (request.command == quickky::GuardCommand::Status) {
                return (attachment->committed ? 1 : 2) |
                    (quickky::NativeTabGuard::CountTabs(window) << 8) |
                    (static_cast<LRESULT>(attachment->guard.blocked()) << 16);
            }
            if (request.command == quickky::GuardCommand::Attach) return 1;
            if (request.command == quickky::GuardCommand::Commit) return Commit() ? 1 : 0;
            if (attachment->identity.workspace == 2) return QueueNativeCommand(request);
            if (request.command == quickky::GuardCommand::Close && attachment->lease && attachment->committed) {
                SendMessageW(window, WM_CLOSE, 0, 0);
                return 1;
            }
            if (request.command == quickky::GuardCommand::Configure && attachment->lease && attachment->committed) {
                return attachment->strip.Configure(request) ? 1 : 0;
            }
            if (request.command == quickky::GuardCommand::Present && attachment->lease && attachment->committed &&
                request.placement.length == sizeof(WINDOWPLACEMENT) && request.visible <= 1) {
                const auto current = attachment;
                current->changed = true;
                if (!request.visible) {
                    current->strip.Hide();
                    ShowWindow(window, SW_HIDE);
                    return !IsWindowVisible(window);
                }
                if (!SetWindowPlacement(window, &request.placement)) return 0;
                if (attachment != current || !ControllerAvailable()) return 0;
                if (current->identity.workspace == 1 && !current->strip.Resize()) {
                    OutputDebugStringA("Quickky: caption Present reflow failed\n");
                    if (attachment == current) Detach();
                    return 0;
                }
                return IsWindowVisible(window) != FALSE;
            }
        }
    }
    // Parent WM_DESTROY precedes host NCDESTROY; never restore a closing root.
    if ((message == WM_DESTROY || message == WM_NCDESTROY) && attachment &&
        attachment->target == window) Detach(false);
    const auto result = DefSubclassProc(window, message, wparam, lparam);
    if (attachment && attachment->target == window && attachment->committed &&
        attachment->identity.workspace != 2 &&
        (message == WM_SIZE || message == WM_DPICHANGED ||
         (attachment->identity.workspace == 1 && (message == WM_WINDOWPOSCHANGED ||
            message == WM_SHOWWINDOW || message == WM_STYLECHANGED || message == WM_DISPLAYCHANGE || message == WM_ENABLE)))) {
        const bool hiding = (message == WM_SHOWWINDOW && !wparam) ||
            (message == WM_SIZE && wparam == SIZE_MINIMIZED) ||
            (message == WM_WINDOWPOSCHANGED && lparam &&
                (reinterpret_cast<const WINDOWPOS*>(lparam)->flags & SWP_HIDEWINDOW));
        const auto current = attachment;
        if (hiding || (message == WM_ENABLE && !wparam)) {
            current->layout_pending = false;
            current->strip.Hide();
        } else if (current->identity.workspace == 1) {
            // WM_WINDOWPOSCHANGED can be nested inside Explorer's placement
            // stack, before its caption hit-test coordinates have caught up.
            current->strip.Hide();
            if (!current->layout_pending) {
                current->layout_pending = true;
                if (!PostMessageW(window, layout_message, 0, 0) && attachment == current) Detach();
            }
        }
        else if (!current->strip.Resize() && attachment == current) {
            char trace[80]{};
            wsprintfA(trace, "Quickky: caption window-message reflow failed message=0x%04X\n", message);
            OutputDebugStringA(trace);
            Detach();
        }
    }
    return result;
}

LRESULT CALLBACK RootProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                               UINT_PTR, DWORD_PTR)
{
    // Win32 calls made by Configure/Resize may destroy the root reentrantly.
    // Detach clears thread ownership, but this method stack must retain its data.
    const auto lifetime = attachment;
    try { return HandleRootMessage(window, message, wparam, lparam); }
    catch (...) {
        Detach();
        return message == WM_COPYDATA ? 0 : DefSubclassProc(window, message, wparam, lparam);
    }
}

bool Attach(HWND target, WPARAM sender, const quickky::GuardRequest& request)
{
    if (attachment) return Matches(target, sender, request);
    if (request.target != reinterpret_cast<std::uint64_t>(target) ||
        GetWindowThreadProcessId(target, nullptr) != GetCurrentThreadId() ||
        InlineIsEqualGUID(request.nonce, kEmptyNonce)) return false;
    wchar_t name[128]{};
    if (!GetClassNameW(target, name, 128) || wcscmp(name, L"CabinetWClass") != 0) return false;
    DWORD sender_pid = 0;
    GetWindowThreadProcessId(reinterpret_cast<HWND>(sender), &sender_pid);
    if (!sender_pid || sender_pid != request.controller_pid) return false;
    ProcessHandle process(OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION,
                                      FALSE, sender_pid), CloseHandle);
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!process || !GetProcessTimes(process.get(), &created, &exited, &kernel, &user) ||
        ((static_cast<std::uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime)
            != request.controller_started_at) return false;

    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
            reinterpret_cast<LPCWSTR>(&RootProcedure), &pinned)) return false;
    auto next = std::make_shared<Attachment>();
    next->target = target;
    next->sender = reinterpret_cast<HWND>(sender);
    next->nonce = request.nonce;
    next->controller = std::move(process);
    next->identity = request;
    if (request.workspace) {
        const auto lease_name = quickky::LeaseEventName(request.nonce);
        next->lease.reset(OpenEventW(SYNCHRONIZE, FALSE, lease_name.c_str()));
        if (!next->lease || WaitForSingleObject(next->lease.get(), 0) != WAIT_TIMEOUT) return false;
    }
    if (request.workspace > 3) return false;
    if (request.workspace == 2) {
        if (!native_message) native_message = RegisterWindowMessageW(L"QuickkyExplorerTabs.Native.v6");
        if (!native_message) return false;
        next->completed.reset(OpenEventW(EVENT_MODIFY_STATE, FALSE,
            (quickky::NativeMappingName(request.nonce) + L".Ready").c_str()));
        if (!next->completed) return false;
        next->mapping.reset(OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE,
            quickky::NativeMappingName(request.nonce).c_str()));
        if (!next->mapping) return false;
        next->exchange = static_cast<quickky::NativeExchange*>(MapViewOfFile(next->mapping.get(),
            FILE_MAP_ALL_ACCESS, 0, 0, sizeof(quickky::NativeExchange)));
        if (!next->exchange) return false;
    } else if (quickky::NativeTabGuard::CountTabs(target) != 1) return false;
    if (request.workspace == 1 && !layout_message) {
        layout_message = RegisterWindowMessageW(L"QuickkyExplorerTabs.Layout.v6");
        if (!layout_message) return false;
    }
    if (GetPropW(target, kBindingProperty) || !SetPropW(target, kBindingProperty, next.get())) return false;
    if (!request.workspace) {
        if (!next->guard.Start(target)) return false;
        next->committed = true;
    }
    if (!SetWindowSubclass(target, RootProcedure, kSubclass, 0)) return false;
    next->timer = SetTimer(nullptr, 0, 500, CheckController);
    if (!next->timer) { RemoveWindowSubclass(target, RootProcedure, kSubclass); return false; }
    attachment = std::move(next);
    return true;
}

} // namespace

extern "C" __declspec(dllexport) LRESULT CALLBACK QuickkyBootstrap(int code, WPARAM wparam, LPARAM lparam)
{
    if (code >= 0) {
        const auto message = reinterpret_cast<const CWPSTRUCT*>(lparam);
        if (message && message->message == WM_COPYDATA) {
            quickky::GuardRequest request;
            if (Decode(message->lParam, request) && request.command == quickky::GuardCommand::Attach) {
                try { Attach(message->hwnd, message->wParam, request); }
                catch (...) { Detach(); }
            }
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

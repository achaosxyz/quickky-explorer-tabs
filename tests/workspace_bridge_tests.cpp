#include "explorer/folder_location.h"
#include "hook/hook_client.h"
#include "settings/preferences.h"

#include <cwchar>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <dwmapi.h>
#include <knownfolders.h>

namespace {

constexpr wchar_t kRootClass[] = L"CabinetWClass";
constexpr wchar_t kTabClass[] = L"ShellTabWindowClass";
constexpr wchar_t kHostClass[] = L"Microsoft.UI.Content.DesktopChildSiteBridge";
constexpr UINT kCreateTabMessage = WM_APP + 0x51;
constexpr UINT kDestroyTabMessage = WM_APP + 0x52;
constexpr UINT kDestroyDuringCommitMessage = WM_APP + 0x53;
constexpr UINT kDestroyDuringResizeMessage = WM_APP + 0x54;
constexpr UINT kDetachDuringClipMessage = WM_APP + 0x55;
constexpr UINT kArmClassicCaptionReflowMessage = WM_APP + 0x56;
constexpr UINT kClearClassicCaptionHitTestMessage = WM_APP + 0x57;
constexpr UINT kClassicCaptionReflowCompleteMessage = WM_APP + 0x58;
constexpr LRESULT kUnprocessedCopyData = 0x514B0000;

void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct TargetState {
    HANDLE ready = nullptr;
    HWND root = nullptr;
    HWND initial_tab = nullptr;
    HWND extra_tab = nullptr;
    HWND host = nullptr;
    HANDLE classic_caption_reflow_complete = nullptr;
    DWORD thread_id = 0;
    bool destroy_on_chrome = false;
    bool destroy_on_resize = false;
    bool destroy_on_text = false;
    bool detach_on_clip = false;
    bool arm_classic_caption_reflow = false;
    bool classic_caption_hit_test_blocked = false;
    HWND detach_sender = nullptr;
    quickky::GuardRequest detach_request;
    HHOOK resize_hook = nullptr;
};

thread_local TargetState* resizing_target = nullptr;

// Simulate another UI-thread integration closing the root during child layout.
LRESULT CALLBACK DestroyDuringResize(int code, WPARAM wparam, LPARAM lparam)
{
    if (code >= 0 && resizing_target &&
        (resizing_target->destroy_on_resize || resizing_target->detach_on_clip)) {
        const auto* message = reinterpret_cast<const CWPSTRUCT*>(lparam);
        wchar_t host_name[128]{};
        if (resizing_target->detach_on_clip && message->message == WM_WINDOWPOSCHANGING &&
            GetClassNameW(message->hwnd, host_name, 128) && !wcscmp(host_name, kHostClass)) {
            resizing_target->detach_on_clip = false;
            COPYDATASTRUCT packet{quickky::kGuardProtocol, sizeof(quickky::GuardRequest),
                &resizing_target->detach_request};
            SendMessageW(resizing_target->root, WM_COPYDATA,
                reinterpret_cast<WPARAM>(resizing_target->detach_sender), reinterpret_cast<LPARAM>(&packet));
        }
        wchar_t name[64]{};
        const bool trigger = resizing_target->destroy_on_text ? message->message == WM_SETTEXT :
            message->message == WM_WINDOWPOSCHANGING;
        if (trigger &&
            GetClassNameW(message->hwnd, name, 64) && wcscmp(name, L"Quickky.TabStrip") == 0) {
            resizing_target->destroy_on_resize = false;
            DestroyWindow(resizing_target->root);
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

LRESULT CALLBACK SyntheticRootProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto* state = reinterpret_cast<TargetState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        state = static_cast<TargetState*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }

    if (message == kArmClassicCaptionReflowMessage && state) {
        state->arm_classic_caption_reflow = true;
        state->classic_caption_hit_test_blocked = false;
        ResetEvent(state->classic_caption_reflow_complete);
        return 1;
    }
    if (message == WM_WINDOWPOSCHANGED && state && state->arm_classic_caption_reflow) {
        state->arm_classic_caption_reflow = false;
        state->classic_caption_hit_test_blocked = true;
        PostMessageW(window, kClearClassicCaptionHitTestMessage, 0, 0);
    }
    if (message == WM_NCHITTEST && state && state->classic_caption_hit_test_blocked) return HTBORDER;
    if (message == kClearClassicCaptionHitTestMessage && state) {
        state->classic_caption_hit_test_blocked = false;
        return 0;
    }
    if (message == kClassicCaptionReflowCompleteMessage && state) {
        // An async native state flag may change before its layout is queued.
        // Relay once behind that work before releasing geometry assertions.
        if (!wparam) PostMessageW(window, kClassicCaptionReflowCompleteMessage, 1, 0);
        else SetEvent(state->classic_caption_reflow_complete);
        return 0;
    }

    if (message == kDestroyDuringCommitMessage && state) {
        state->destroy_on_chrome = true;
        return 1;
    }
    if (message == kDetachDuringClipMessage && state) {
        resizing_target = state;
        state->detach_sender = reinterpret_cast<HWND>(wparam);
        state->detach_request = *reinterpret_cast<const quickky::GuardRequest*>(lparam);
        state->detach_request.command = quickky::GuardCommand::Detach;
        state->detach_on_clip = true;
        state->resize_hook = SetWindowsHookExW(WH_CALLWNDPROC, DestroyDuringResize, nullptr, GetCurrentThreadId());
        return state->resize_hook != nullptr;
    }
    if (message == kDestroyDuringResizeMessage && state) {
        resizing_target = state;
        state->resize_hook = SetWindowsHookExW(WH_CALLWNDPROC, DestroyDuringResize,
            nullptr, GetCurrentThreadId());
        if (!state->resize_hook) return 0;
        state->destroy_on_resize = true;
        state->destroy_on_text = wparam == 2;
        if (wparam == 0) {
            SendMessageW(window, WM_SIZE, SIZE_RESTORED, MAKELPARAM(640, 480));
        }
        return 1;
    }
    if (message == WM_PARENTNOTIFY && LOWORD(wparam) == WM_CREATE && state && state->destroy_on_chrome) {
        wchar_t name[64]{};
        GetClassNameW(reinterpret_cast<HWND>(lparam), name, 64);
        if (wcscmp(name, L"Quickky.TabStrip") == 0) {
            DestroyWindow(window);
            return 0;
        }
    }
    if (message == kCreateTabMessage && state) {
        state->extra_tab = CreateWindowExW(0, kTabClass, L"", WS_CHILD,
            0, 0, 100, 100, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        return reinterpret_cast<LRESULT>(state->extra_tab);
    }
    if (message == kDestroyTabMessage && state && state->extra_tab) {
        const HWND tab = state->extra_tab;
        state->extra_tab = nullptr;
        return DestroyWindow(tab);
    }
    if (message == WM_COPYDATA) return kUnprocessedCopyData;
    if (message == WM_CLOSE) {
        DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void RegisterSyntheticClasses()
{
    const auto instance = GetModuleHandleW(nullptr);
    WNDCLASSW root{};
    root.hInstance = instance;
    root.lpfnWndProc = SyntheticRootProcedure;
    root.lpszClassName = kRootClass;
    Check(RegisterClassW(&root) != 0, "Register synthetic CabinetWClass");

    WNDCLASSW tab{};
    tab.hInstance = instance;
    tab.lpfnWndProc = DefWindowProcW;
    tab.lpszClassName = kTabClass;
    Check(RegisterClassW(&tab) != 0, "Register synthetic ShellTabWindowClass");
    tab.lpszClassName = kHostClass;
    Check(RegisterClassW(&tab) != 0, "Register synthetic native header host");
}

DWORD WINAPI TargetThread(void* parameter)
{
    auto* state = static_cast<TargetState*>(parameter);
    state->thread_id = GetCurrentThreadId();
    const auto instance = GetModuleHandleW(nullptr);
    state->root = CreateWindowExW(0, kRootClass, L"Quickky synthetic root", 0,
        0, 0, 640, 480, nullptr, nullptr, instance, state);
    if (state->root) {
        RECT bounds{};
        GetClientRect(state->root, &bounds);
        state->host = CreateWindowExW(0, kHostClass, L"", WS_CHILD | WS_VISIBLE,
            0, 0, bounds.right, 180, state->root, nullptr, instance, nullptr);
        state->initial_tab = CreateWindowExW(0, kTabClass, L"", WS_CHILD,
            0, 0, 100, 100, state->root, nullptr, instance, nullptr);
    }
    SetEvent(state->ready);
    if (!state->root || !state->initial_tab) {
        if (state->root) DestroyWindow(state->root);
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (state->resize_hook) UnhookWindowsHookEx(state->resize_hook);
    resizing_target = nullptr;
    return 0;
}

class SyntheticTarget {
public:
    SyntheticTarget()
    {
        state_.ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Check(state_.ready != nullptr, "Create synthetic target ready event");
        thread_ = CreateThread(nullptr, 0, TargetThread, &state_, 0, nullptr);
        if (!thread_) {
            CloseHandle(state_.ready);
            state_.ready = nullptr;
            Check(false, "Start synthetic target thread");
        }
        const auto wait = WaitForSingleObject(state_.ready, 5000);
        if (wait != WAIT_OBJECT_0 || !state_.root || !state_.initial_tab) {
            Stop();
            Check(false, "Create hidden synthetic root with one native child");
        }
        Check(!IsWindowVisible(state_.root) && !IsWindowVisible(state_.initial_tab),
            "Synthetic windows must remain hidden");
    }

    ~SyntheticTarget() { Stop(); }
    SyntheticTarget(const SyntheticTarget&) = delete;
    SyntheticTarget& operator=(const SyntheticTarget&) = delete;

    HWND root() const noexcept { return state_.root; }
    HWND initial_tab() const noexcept { return state_.initial_tab; }
    DWORD thread_id() const noexcept { return state_.thread_id; }

    HWND CreateExtraTab()
    {
        return reinterpret_cast<HWND>(Send(kCreateTabMessage));
    }

    void DestroyDuringCommit()
    {
        Check(Send(kDestroyDuringCommitMessage) != 0, "Arm reentrant destruction during chrome creation");
    }

    void ArmClassicCaptionReflowRace()
    {
        state_.classic_caption_reflow_complete = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        Check(state_.classic_caption_reflow_complete != nullptr,
            "Create classic caption reflow completion event");
        Check(Send(kArmClassicCaptionReflowMessage) != 0,
            "Arm temporary non-caption hit tests for classic reflow");
    }

    bool WaitForClassicCaptionReflow()
    {
        // Geometry assertions follow posted layout, not just the native state flag.
        ResetEvent(state_.classic_caption_reflow_complete);
        return PostMessageW(state_.root, kClassicCaptionReflowCompleteMessage, 0, 0) &&
            WaitForSingleObject(state_.classic_caption_reflow_complete, 3000) == WAIT_OBJECT_0;
    }

    void DestroyDuringResize(bool child_message)
    {
        DWORD_PTR result = 0;
        SendMessageTimeoutW(state_.root, kDestroyDuringResizeMessage, child_message ? 1 : 0, 0,
            SMTO_ABORTIFHUNG, 3000, &result);
        if (child_message) {
            const auto strip = FindWindowExW(state_.root, nullptr, L"Quickky.TabStrip", nullptr);
            Check(strip != nullptr, "Find presenter for direct child theme callback");
            // Send outside any root callback so only the child's weak keepalive protects it.
            SendMessageTimeoutW(strip, WM_THEMECHANGED, 0, 0, SMTO_ABORTIFHUNG, 3000, &result);
        }
        Check(WaitForSingleObject(thread_, 1000) == WAIT_OBJECT_0 && !IsWindow(state_.root),
            child_message ? "Close root during direct child theme layout" :
                "Close root during committed root resize");
    }

    void ArmDestructionDuringConfigure()
    {
        DWORD_PTR result = 0;
        Check(SendMessageTimeoutW(state_.root, kDestroyDuringResizeMessage, 2, 0,
            SMTO_ABORTIFHUNG, 3000, &result) && result == 1,
            "Arm synthetic root destruction during label configuration");
    }

    void DestroyExtraTab()
    {
        Check(Send(kDestroyTabMessage) != 0, "Destroy synthetic extra tab");
    }

private:
    DWORD_PTR Send(UINT message)
    {
        DWORD_PTR result = 0;
        Check(SendMessageTimeoutW(state_.root, message, 0, 0,
            SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 3000, &result) != 0,
            "Send command to synthetic target thread");
        return result;
    }

    void Stop() noexcept
    {
        if (thread_) {
            if (state_.root && IsWindow(state_.root)) PostMessageW(state_.root, WM_CLOSE, 0, 0);
            else if (state_.thread_id) PostThreadMessageW(state_.thread_id, WM_QUIT, 0, 0);
            WaitForSingleObject(thread_, INFINITE);
            CloseHandle(thread_);
            thread_ = nullptr;
        }
        if (state_.ready) {
            CloseHandle(state_.ready);
            state_.ready = nullptr;
        }
        if (state_.classic_caption_reflow_complete) {
            CloseHandle(state_.classic_caption_reflow_complete);
            state_.classic_caption_reflow_complete = nullptr;
        }
    }

    TargetState state_;
    HANDLE thread_ = nullptr;
};

class ControllerWindow {
public:
    ControllerWindow()
    {
        window_ = CreateWindowExW(0, L"STATIC", L"", 0, 0, 0, 0, 0,
            HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
        Check(window_ != nullptr, "Create message-only controller window");
    }
    ~ControllerWindow() { if (window_) DestroyWindow(window_); }
    HWND get() const noexcept { return window_; }
private:
    HWND window_ = nullptr;
};

struct Delivery {
    bool delivered = false;
    DWORD_PTR result = 0;
};

Delivery SendPacket(HWND target, HWND sender, const quickky::GuardRequest& request,
    DWORD byte_count = sizeof(quickky::GuardRequest), ULONG_PTR protocol = quickky::kGuardProtocol)
{
    COPYDATASTRUCT data{protocol, byte_count, const_cast<quickky::GuardRequest*>(&request)};
    DWORD_PTR result = 0;
    const auto delivered = SendMessageTimeoutW(target, WM_COPYDATA,
        reinterpret_cast<WPARAM>(sender), reinterpret_cast<LPARAM>(&data),
        SMTO_ABORTIFHUNG | SMTO_ERRORONEXIT, 3000, &result) != 0;
    return {delivered, result};
}

void CheckRejected(const Delivery& delivery, const char* message)
{
    Check(delivery.delivered && delivery.result == static_cast<DWORD_PTR>(kUnprocessedCopyData), message);
}

std::uint64_t CurrentProcessStartTime()
{
    FILETIME created{}, exited{}, kernel{}, user{};
    Check(GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) != 0,
        "Read test process start time");
    return (static_cast<std::uint64_t>(created.dwHighDateTime) << 32) | created.dwLowDateTime;
}

class GuardOnlyProbe {
public:
    GuardOnlyProbe(HWND target, HWND sender, DWORD target_thread, HMODULE module)
        : target_(target), sender_(sender)
    {
        request_.controller_pid = GetCurrentProcessId();
        request_.controller_started_at = CurrentProcessStartTime();
        request_.target = reinterpret_cast<std::uint64_t>(target);
        Check(SUCCEEDED(CoCreateGuid(&request_.nonce)), "Create guard-only nonce");
        const auto callback = reinterpret_cast<HOOKPROC>(GetProcAddress(module, "QuickkyBootstrap"));
        hook_ = callback ? SetWindowsHookExW(WH_CALLWNDPROC, callback, module, target_thread) : nullptr;
        Check(hook_ != nullptr, "Install synthetic target-thread bootstrap hook");
    }

    ~GuardOnlyProbe()
    {
        if (attached_) Detach();
        if (hook_) UnhookWindowsHookEx(hook_);
    }

    quickky::GuardRequest& request() noexcept { return request_; }
    Delivery Send(DWORD byte_count = sizeof(quickky::GuardRequest),
        ULONG_PTR protocol = quickky::kGuardProtocol)
    {
        return SendPacket(target_, sender_, request_, byte_count, protocol);
    }

    Delivery Attach()
    {
        request_.command = quickky::GuardCommand::Attach;
        const auto delivery = Send();
        attached_ = delivery.delivered && delivery.result == 1;
        return delivery;
    }

    bool Detach()
    {
        if (!attached_) return true;
        request_.command = quickky::GuardCommand::Detach;
        const auto delivery = Send();
        attached_ = false;
        return delivery.delivered && delivery.result == 1;
    }

private:
    HWND target_ = nullptr;
    HWND sender_ = nullptr;
    HHOOK hook_ = nullptr;
    quickky::GuardRequest request_;
    bool attached_ = false;
};

void TestDirectoryBoundaryAndLocale()
{
    const std::wstring root = L"C:\\Users\\Fixture\\Workspace";
    Check(quickky::IsWithinDirectory(root, root), "Accept exact directory boundary");
    Check(quickky::IsWithinDirectory(L"c:\\users\\fixture\\workspace\\Folder", root),
        "Accept case-insensitive descendant directory");
    Check(!quickky::IsWithinDirectory(L"C:\\Users\\Fixture\\Workspace2", root),
        "Reject sibling sharing a directory prefix");
    Check(!quickky::IsWithinDirectory(L"C:\\Users\\Fixture\\Other", root),
        "Reject path outside the directory boundary");
    Check(!quickky::IsWithinDirectory(L"C:\\Users\\Fixture\\Workspace", L""),
        "Reject empty directory root");

    const auto english = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
    const auto chinese = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED);
    const auto german = MAKELANGID(LANG_GERMAN, SUBLANG_GERMAN);
    Check(quickky::ResolveLocale(L"zh-CN", english) == L"zh-CN",
        "Explicit Chinese locale takes priority");
    Check(quickky::ResolveLocale(L"en-US", chinese) == L"en-US",
        "Explicit English locale takes priority");
    Check(quickky::ResolveLocale(L"", chinese) == L"zh-CN",
        "Chinese system language resolves to Chinese");
    Check(quickky::ResolveLocale(L"", english) == L"en-US",
        "English system language resolves to English");
    Check(quickky::ResolveLocale(L"fr-FR", german) == L"en-US",
        "Unsupported locale falls back to English");
}

void TestGuardOnlyProtocol(SyntheticTarget& target, HWND sender, HMODULE module)
{
    GuardOnlyProbe probe(target.root(), sender, target.thread_id(), module);
    auto& request = probe.request();
    request.version = quickky::kGuardVersion + 1;
    CheckRejected(probe.Send(), "Reject guard attach with mismatched protocol version");
    request.version = quickky::kGuardVersion;
    request.size = static_cast<std::uint32_t>(sizeof(request) - 1);
    CheckRejected(probe.Send(), "Reject malformed guard request size");
    request.size = sizeof(request);
    CheckRejected(probe.Send(sizeof(request) - 1), "Reject truncated guard packet");

    Check(probe.Attach().result == 1, "Attach guard-only synthetic target");
    request.command = quickky::GuardCommand::Status;
    auto status = probe.Send();
    Check(status.delivered && status.result == 0x101,
        "Report healthy guard with exactly one native child");

    request.version = quickky::kGuardVersion + 1;
    CheckRejected(probe.Send(), "Reject status with mismatched protocol version");
    request.version = quickky::kGuardVersion;
    request.size = static_cast<std::uint32_t>(sizeof(request) - 1);
    CheckRejected(probe.Send(), "Reject malformed status request");
    request.size = sizeof(request);
    request.nonce.Data1 ^= 1;
    CheckRejected(probe.Send(), "Reject status with mismatched nonce");
    request.nonce.Data1 ^= 1;
    CheckRejected(probe.Send(sizeof(request), quickky::kGuardProtocol + 1),
        "Reject status with mismatched protocol tag");
    status = probe.Send();
    Check(status.delivered && status.result == 0x101,
        "Keep guard healthy after rejected protocol requests");

    Check(probe.Detach(), "Detach guard-only synthetic target");
    const HWND extra = target.CreateExtraTab();
    Check(extra != nullptr, "Allow native child creation after guard detach");
    target.DestroyExtraTab();
}

quickky::WindowIdentity MakeIdentity(const SyntheticTarget& target)
{
    quickky::WindowIdentity identity;
    identity.window.handle = reinterpret_cast<std::uintptr_t>(target.root());
    identity.window.process_id = GetCurrentProcessId();
    identity.window.thread_id = target.thread_id();
    identity.window.process_started_at = CurrentProcessStartTime();
    identity.generation = 1;
    return identity;
}

void CheckNativeBand(HWND root, bool clipped)
{
    const auto host = FindWindowExW(root, nullptr, kHostClass, nullptr);
    auto region = CreateRectRgn(0, 0, 0, 0);
    Check(host && region, "Inspect synthetic native band");
    const auto type = GetWindowRgn(host, region);
    const auto scale = GetDpiForWindow(root) / 96.0f;
    const bool hidden = type != ERROR && !PtInRegion(region, 10, 10) &&
        PtInRegion(region, 10, static_cast<int>(60 * scale));
    DeleteObject(region);
    Check(clipped ? hidden : type == ERROR, clipped ?
        "Suppress native band while preserving navigation" : "Restore native host default region");
}

void TestWorkspaceBridge(SyntheticTarget& target, HWND sender, HMODULE module,
    const quickky::WindowIdentity& identity)
{
    {
        quickky::HookClient prepared;
        Check(prepared.Prepare(sender, module, identity, quickky::WorkspaceMode::Diagnostic), "Prepare binding to synthetic target");
        Check(!prepared.Healthy(), "Prepared binding is not a committed workspace");
        CheckNativeBand(target.root(), false);
        Check(FindWindowExW(target.root(), nullptr, L"Quickky.TabStrip", nullptr) == nullptr,
            "Preparation must not create visible or hidden chrome");
        Check(target.CreateExtraTab() != nullptr, "Preparation must not intercept native creation");
        target.DestroyExtraTab();
    }
    quickky::HookClient client;
    Check(client.Prepare(sender, module, identity, quickky::WorkspaceMode::Diagnostic), "Prepare workspace after native behavior probe");
    Check(client.Commit(), "Commit workspace after caller validation");
    Check(client.Healthy(), "Confirm attached workspace is healthy");
    CheckNativeBand(target.root(), true);

    quickky::GuardRequest tabs;
    tabs.count = 2;
    tabs.active = 22;
    tabs.tabs[0].id = 11;
    std::wmemcpy(tabs.tabs[0].title, L"First", 6);
    tabs.tabs[1].id = 22;
    std::wmemcpy(tabs.tabs[1].title, L"Second", 7);
    Check(client.Configure(tabs), "Configure valid workspace tabs");
    const HWND strip = FindWindowExW(target.root(), nullptr, L"Quickky.TabStrip", nullptr);
    Check(strip != nullptr, "Create hidden workspace tab strip");
    Check(!IsWindowVisible(target.root()) && !IsWindowVisible(strip),
        "Keep workspace and tab strip hidden");

    auto too_many = tabs;
    too_many.count = quickky::kMaxWorkspaceTabs + 1;
    Check(!client.Configure(too_many), "Reject workspace count above protocol limit");
    auto unterminated = tabs;
    unterminated.count = 1;
    for (auto& character : unterminated.tabs[0].title) character = L'X';
    Check(!client.Configure(unterminated), "Reject unterminated workspace label");
    Check(client.Healthy(), "Keep workspace healthy after invalid configuration");

    Check(target.CreateExtraTab() == nullptr, "Block extra native child while workspace is attached");
    Check(!IsWindowVisible(target.root()), "Keep root hidden while native child creation is blocked");
    Check(client.Detach(), "Normally detach workspace client");
    Check(!client.Healthy(), "Report detached workspace as inactive");
    CheckNativeBand(target.root(), false);
    Check(FindWindowExW(target.root(), nullptr, L"Quickky.TabStrip", nullptr) == nullptr,
        "Remove workspace tab strip on detach");

    const HWND extra = target.CreateExtraTab();
    Check(extra != nullptr, "Restore native child creation after normal detach");
    target.DestroyExtraTab();
    Check(!IsWindowVisible(target.root()), "Keep synthetic root hidden after normal detach");
}

bool PumpUntilWatchdogDetaches(quickky::HookClient& client)
{
    const ULONGLONG deadline = GetTickCount64() + 1600;
    while (GetTickCount64() < deadline) {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        if (!FindWindowExW(reinterpret_cast<HWND>(client.window().window.handle), nullptr,
                L"Quickky.TabStrip", nullptr)) return !client.Healthy();
        MsgWaitForMultipleObjectsEx(0, nullptr, 25, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }
    return !FindWindowExW(reinterpret_cast<HWND>(client.window().window.handle), nullptr,
        L"Quickky.TabStrip", nullptr) && !client.Healthy();
}

void TestLeaseWatchdog(SyntheticTarget& target, HWND sender, HMODULE module,
    const quickky::WindowIdentity& identity)
{
    quickky::HookClient client;
    Check(client.Prepare(sender, module, identity, quickky::WorkspaceMode::Diagnostic) && client.Commit(),
        "Attach workspace for lease watchdog test");
    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    Check(GetWindowPlacement(target.root(), &placement) != 0, "Read hidden root placement");
    Check(client.Present(placement, false), "Apply hidden workspace placement");
    Check(!IsWindowVisible(target.root()), "Keep root hidden before lease cancellation");

    client.Cancel();
    Check(PumpUntilWatchdogDetaches(client), "Recover workspace after lease cancellation watchdog");
    CheckNativeBand(target.root(), false);
    Check(!IsWindowVisible(target.root()), "Restore originally hidden root after watchdog recovery");
    Check(FindWindowExW(target.root(), nullptr, L"Quickky.TabStrip", nullptr) == nullptr,
        "Remove workspace tab strip after watchdog recovery");
    const HWND extra = target.CreateExtraTab();
    Check(extra != nullptr, "Restore native child creation after watchdog recovery");
    target.DestroyExtraTab();
}

void TestRejectedCommit(SyntheticTarget& target, HWND sender, HMODULE module,
    const quickky::WindowIdentity& identity)
{
    {
        quickky::HookClient client;
        Check(client.Prepare(sender, module, identity, quickky::WorkspaceMode::Diagnostic), "Prepare before native view count changes");
        Check(target.CreateExtraTab() != nullptr, "Allow original native behavior before commit");
        Check(!client.Commit(), "Reject commit after native view count changes");
        Check(FindWindowExW(target.root(), nullptr, L"Quickky.TabStrip", nullptr) == nullptr,
            "Failed commit must not create chrome");
        target.DestroyExtraTab();
    }
    {
        quickky::HookClient client;
        Check(client.Prepare(sender, module, identity, quickky::WorkspaceMode::Diagnostic), "Prepare before lease cancellation");
        client.Cancel();
        Check(!client.Commit(), "Cancelled preparation must not commit");
        Check(target.CreateExtraTab() != nullptr, "Cancelled preparation keeps native behavior");
        target.DestroyExtraTab();
    }
}

void TestAuthenticatedClose(HWND sender, HMODULE module)
{
    SyntheticTarget target;
    quickky::HookClient client;
    Check(client.Prepare(sender, module, MakeIdentity(target), quickky::WorkspaceMode::Diagnostic) && client.Commit(),
        "Commit separate hidden target for close test");
    client.RequestClose();
    Check(!IsWindow(target.root()), "Authenticated close destroys only its bound target");
}

void TestDestructionDuringCommit(HWND sender, HMODULE module)
{
    SyntheticTarget target;
    quickky::HookClient client;
    Check(client.Prepare(sender, module, MakeIdentity(target), quickky::WorkspaceMode::Diagnostic), "Prepare target that will close during commit");
    target.DestroyDuringCommit();
    Check(!client.Commit(), "Reject commit when parent closes during child initialization");
    Check(!IsWindow(target.root()), "Keep destroyed target closed without crashing the host process");
}

void TestDestructionDuringResize(HWND sender, HMODULE module)
{
    for (const bool child_message : {false, true}) {
        SyntheticTarget target;
        quickky::HookClient client;
        Check(client.Prepare(sender, module, MakeIdentity(target), quickky::WorkspaceMode::Diagnostic) && client.Commit(),
            "Commit target before reentrant layout teardown");
        target.DestroyDuringResize(child_message);
        Check(!client.Healthy(), "Destroyed layout target must no longer accept workspace commands");
    }
}

void TestDestructionDuringConfigure(HWND sender, HMODULE module)
{
    SyntheticTarget target;
    quickky::HookClient client;
    Check(client.Prepare(sender, module, MakeIdentity(target), quickky::WorkspaceMode::Diagnostic) && client.Commit(),
        "Commit target before reentrant configuration teardown");
    target.ArmDestructionDuringConfigure();
    quickky::GuardRequest tabs;
    tabs.count = 1;
    tabs.active = tabs.tabs[0].id = 1;
    client.Configure(tabs);
    Check(!IsWindow(target.root()) && !client.Healthy(),
        "Reentrant label changes must not access a destroyed presenter");
}

void TestNativeHostLoss(HWND sender, HMODULE module)
{
    SyntheticTarget target;
    quickky::HookClient client;
    Check(client.Prepare(sender, module, MakeIdentity(target), quickky::WorkspaceMode::Diagnostic) && client.Commit(), "Commit before native host loss");
    const auto host = FindWindowExW(target.root(), nullptr, kHostClass, nullptr);
    SendMessageW(host, WM_CLOSE, 0, 0);
    Check(!client.Healthy() && !FindWindowExW(target.root(), nullptr, L"Quickky.TabStrip", nullptr),
        "Native host loss must remove custom chrome immediately");
    quickky::HookClient unknown;
    Check(unknown.Prepare(sender, module, MakeIdentity(target), quickky::WorkspaceMode::Diagnostic) && !unknown.Commit(),
        "Missing native host must reject commit before custom drawing");
}

void TestFolderAdmission()
{
    Check(SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED)), "Initialize read-only Shell metadata tests");
    struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
    unsigned namespace_case = 0;
    const auto parse = [&namespace_case](const wchar_t* name) {
        ++namespace_case;
        PIDLIST_ABSOLUTE value = nullptr;
        const auto result = SHParseDisplayName(name, nullptr, &value, 0, nullptr);
        if (FAILED(result)) std::cerr << "Namespace fixture " << namespace_case << " HRESULT " << std::hex << result << std::dec << '\n';
        Check(SUCCEEDED(result) && value,
            "Resolve a namespace PIDL without opening an Explorer window");
        return quickky::OwnedPidl(value);
    };
    for (const auto id : {&FOLDERID_ControlPanelFolder, &FOLDERID_ConnectionsFolder, &FOLDERID_PrintersFolder}) {
        PIDLIST_ABSOLUTE value = nullptr;
        Check(SUCCEEDED(SHGetKnownFolderIDList(*id, 0, nullptr, &value)) && value,
            "Resolve a system management namespace");
        Check(!quickky::DescribeFolder(quickky::OwnedPidl(value)), "Exclude management namespaces from collection");
    }
    for (const auto name : {
        L"::{26EE0668-A00A-44D7-9371-BEB064C98683}",
        L"::{21ec2020-3aea-1069-a2dd-08002b30309d}",
        L"::{26EE0668-A00A-44D7-9371-BEB064C98683}\\0\\::{7007ACC7-3202-11D1-AAD2-00805FC1270E}",
        L"::{21EC2020-3AEA-1069-A2DD-08002B30309D}\\::{2227A280-3AEA-1069-A2DE-08002B30309D}",
        L"shell:::{7B81BE6A-CE2B-4676-A29E-EB907A5126C5}"}) {
        Check(!quickky::DescribeFolder(parse(name)), "Exclude root aliases, descendants and directly addressed applets");
    }
    for (const auto id : {&FOLDERID_ComputerFolder, &FOLDERID_Libraries, &FOLDERID_RecycleBinFolder, &FOLDERID_NetworkFolder}) {
        PIDLIST_ABSOLUTE value = nullptr;
        Check(SUCCEEDED(SHGetKnownFolderIDList(*id, 0, nullptr, &value)) && value,
            "Resolve a virtual browsing root");
        Check(quickky::DescribeFolder(quickky::OwnedPidl(value)).has_value(),
            "Keep normal virtual browsing roots eligible");
    }
    for (const auto id : {L"{F874310E-B6B7-47DC-BC84-B9E6B38F5903}", L"{679F85CB-0220-4080-B29B-5540CC05AAB6}"}) {
        PIDLIST_ABSOLUTE value = nullptr;
        const auto result = SHParseDisplayName((std::wstring(L"shell:::") + id).c_str(), nullptr, &value, 0, nullptr);
        quickky::OwnedPidl pidl(value);
        if (SUCCEEDED(result) && pidl) {
            Check(quickky::DescribeFolder(std::move(pidl)).has_value(), "Keep parseable Home/Quick Access roots eligible");
        } else {
            // Registration alone does not guarantee standalone desktop parsing.
            std::cout << "SKIP: Home/Quick Access namespace is not independently parseable on this host\n";
        }
    }
    wchar_t temporary[MAX_PATH]{}, suffix[39]{};
    GUID nonce{};
    const auto length = GetTempPathW(MAX_PATH, temporary);
    Check(length && length < MAX_PATH && SUCCEEDED(CoCreateGuid(&nonce)) && StringFromGUID2(nonce, suffix, 39),
        "Create an isolated folder-policy fixture name");
    struct Fixture {
        std::wstring root, ordinary, junction;
        bool owned = false;
        ~Fixture() {
            if (owned) { RemoveDirectoryW(junction.c_str()); RemoveDirectoryW(ordinary.c_str()); RemoveDirectoryW(root.c_str()); }
        }
    } fixture;
    fixture.root = std::wstring(temporary) + L"QuickkyFolderPolicy-" + suffix;
    fixture.ordinary = fixture.root + L"\\Control Panel";
    fixture.junction = fixture.root + L"\\Tasks.{ed7ba470-8e54-465e-825c-99712043e01c}";
    fixture.owned = CreateDirectoryW(fixture.root.c_str(), nullptr) != FALSE;
    Check(fixture.owned && CreateDirectoryW(fixture.ordinary.c_str(), nullptr) &&
        CreateDirectoryW(fixture.junction.c_str(), nullptr), "Create only owned empty fixture folders");
    Check(quickky::DescribeFolder(parse(fixture.ordinary.c_str())).has_value(),
        "Do not blacklist ordinary folders by their display names");
    Check(!quickky::DescribeFolder(parse(fixture.junction.c_str())), "Exclude a Control Panel namespace junction");
    wchar_t executable[MAX_PATH]{};
    Check(GetModuleFileNameW(nullptr, executable, MAX_PATH) && !quickky::DescribeFolder(parse(executable)),
        "Reject non-folder Shell items");
}

void TestFolderSnapshots()
{
    unsigned char bytes[]{4, 0, 0x42, 0x12, 0, 0};
    auto clone = quickky::CloneFolderSnapshot(bytes, sizeof(bytes));
    Check(clone && !std::memcmp(clone.get(), bytes, sizeof(bytes)), "Clone bounded PIDL snapshot");
    bytes[2] = 0;
    Check(reinterpret_cast<unsigned char*>(clone.get())[2] == 0x42,
        "Queued folder snapshot must own bytes independently of sender/navigation");
    const unsigned char short_item[]{1, 0, 0, 0};
    const unsigned char overrun[]{7, 0, 0, 0};
    const unsigned char no_terminator[]{2, 0};
    const unsigned char trailing[]{0, 0, 0};
    Check(!quickky::CloneFolderSnapshot(short_item, sizeof(short_item)) &&
        !quickky::CloneFolderSnapshot(overrun, sizeof(overrun)) &&
        !quickky::CloneFolderSnapshot(no_terminator, sizeof(no_terminator)) &&
        !quickky::CloneFolderSnapshot(trailing, sizeof(trailing)) &&
        !quickky::CloneFolderSnapshot(nullptr, 2) &&
        !quickky::CloneFolderSnapshot(bytes, quickky::kMaxFolderSnapshotBytes + 1),
        "Reject malformed or oversized snapshots before native PIDL traversal");
}

void TestUnsupportedNativeFrame(HWND sender, HMODULE module)
{
    SyntheticTarget target;
    quickky::HookClient client;
    Check(client.Prepare(sender, module, MakeIdentity(target), quickky::WorkspaceMode::Native),
        "Prepare native transport without mutating a synthetic root");
    Check(!client.Healthy() && !client.Commit(),
        "Reject a root without the matched native frame ABI");
    Check(IsWindow(target.root()) && FindWindowExW(target.root(), nullptr, L"Quickky.TabStrip", nullptr) == nullptr,
        "Unsupported native frames must remain intact without custom chrome");
    CheckNativeBand(target.root(), false);
    Check(target.CreateExtraTab() != nullptr, "Native rejection must not intercept tab creation");
    target.DestroyExtraTab();
}

HWND OwnedStrip(HWND owner)
{
    struct Search { HWND owner; HWND result = nullptr; } search{owner};
    EnumWindows([](HWND window, LPARAM parameter) -> BOOL {
        auto& value = *reinterpret_cast<Search*>(parameter);
        wchar_t name[64]{};
        if (GetWindow(window, GW_OWNER) == value.owner && GetClassNameW(window, name, 64) &&
            wcscmp(name, L"Quickky.TabStrip") == 0) { value.result = window; return FALSE; }
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.result;
}

// Explicit interactive fixture: no real Explorer, global setting, or baseline
// window is touched. Kept out of CTest so headless runners stay supported.
bool Above(HWND upper, HWND lower)
{
    for (unsigned i = 0; lower && i < 512; ++i) {
        lower = GetWindow(lower, GW_HWNDPREV);
        if (lower == upper) return true;
    }
    return false;
}

void TestClassicCaption(HWND sender, HMODULE module)
{
    SyntheticTarget target;
    {
        quickky::HookClient hidden;
        Check(hidden.Prepare(sender, module, MakeIdentity(target)) && !hidden.Commit(),
            "Reject undefined hidden classic-caption geometry without creating UI");
    }
    Check(!OwnedStrip(target.root()), "Rejected classic root has no popup");
    const auto host = FindWindowExW(target.root(), nullptr, kHostClass, nullptr);
    SendMessageW(host, WM_CLOSE, 0, 0);
    Check(!IsWindow(host), "Classic fixture has no Win11 XAML host");
    SetWindowLongPtrW(target.root(), GWL_STYLE, WS_OVERLAPPEDWINDOW);
    Check(SetWindowPos(target.root(), nullptr, 64, 64, 720, 440,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED) != FALSE, "Set classic fixture frame");
    ShowWindowAsync(target.root(), SW_SHOWNOACTIVATE);
    for (unsigned i = 0; i < 40 && !IsWindowVisible(target.root()); ++i) Sleep(25);
    Check(IsWindowVisible(target.root()), "Show only the new synthetic caption fixture");
    DwmFlush();
    RECT client_before{};
    GetClientRect(target.root(), &client_before);
    struct Blocker { HWND value = nullptr; ~Blocker() { if (value) DestroyWindow(value); } } blocker;
    blocker.value = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"Caption fixture blocker",
        WS_POPUP | WS_VISIBLE, 40, 40, 800, 90, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(blocker.value != nullptr, "Create only a synthetic foreground z-order blocker");
    quickky::HookClient client;
    Check(client.Prepare(sender, module, MakeIdentity(target)) && client.Commit(),
        "Commit classic presenter without a XAML host");
    const auto popup = OwnedStrip(target.root());
    Check(popup && IsWindowVisible(popup), "Classic presenter is an owned visible surface");
    Check(Above(blocker.value, popup), "Initial caption never rises above an unrelated foreground surface");
    Check((GetWindowLongPtrW(popup, GWL_STYLE) & WS_CHILD) == 0 &&
        (GetWindowLongPtrW(popup, GWL_EXSTYLE) & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE)) ==
            (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE), "Caption tool window has no independent activation/taskbar role");
    RECT client_after{}, strip_before{}, root_before{};
    GetClientRect(target.root(), &client_after);
    Check(EqualRect(&client_before, &client_after) != FALSE, "Classic attachment never shrinks native client content");
    GetWindowRect(popup, &strip_before);
    GetWindowRect(target.root(), &root_before);
    POINT client_origin{};
    ClientToScreen(target.root(), &client_origin);
    Check(strip_before.bottom <= client_origin.y, "Caption surface never covers file/navigation content");
    quickky::GuardRequest tabs;
    tabs.count = 1; tabs.active = 1; tabs.tabs[0].id = 1;
    std::wmemcpy(tabs.tabs[0].title, L"Classic", 8);
    Check(client.Configure(tabs), "Configure classic caption tabs");
    target.ArmClassicCaptionReflowRace();
    Check(SetWindowPos(target.root(), nullptr, 120, 128, 760, 460, SWP_NOACTIVATE | SWP_NOZORDER) != FALSE,
        "Move/resize only the synthetic owner");
    Check(target.WaitForClassicCaptionReflow() && client.Healthy(),
        "Posted caption reflow waits for transient non-caption hits to clear");
    RECT strip_after{}, root_after{};
    GetWindowRect(popup, &strip_after); GetWindowRect(target.root(), &root_after);
    Check(strip_after.left - strip_before.left == root_after.left - root_before.left &&
        strip_after.top - strip_before.top == root_after.top - root_before.top, "Caption follows its moving owner");
    Check(SetWindowPos(target.root(), nullptr, 40, 48, 700, 420, SWP_NOACTIVATE | SWP_NOZORDER) != FALSE,
        "Move the synthetic owner up/left without stale compositor coordinates");
    Check(target.WaitForClassicCaptionReflow(), "Drain the posted upward/leftward reflow");
    RECT moved{};
    GetWindowRect(popup, &moved);
    Check(moved.left - strip_after.left == 40 - root_after.left &&
        moved.top - strip_after.top == 48 - root_after.top, "Caption follows upward/leftward movement");
    SendMessageW(target.root(), WM_DISPLAYCHANGE, 32, MAKELPARAM(1920, 1080));
    Check(target.WaitForClassicCaptionReflow(), "Drain the posted display reflow");
    Check(Above(blocker.value, popup), "Background display reflow preserves caption z-order");
    EnableWindow(target.root(), FALSE);
    Check(!IsWindowVisible(popup), "Disabled/modal owner cannot expose active caption controls");
    EnableWindow(target.root(), TRUE);
    Check(target.WaitForClassicCaptionReflow(), "Drain the posted enabled-owner reflow");
    Check(IsWindowVisible(popup) && Above(blocker.value, popup), "Re-enabling an owner preserves its z-order band");
    ShowWindowAsync(target.root(), SW_MAXIMIZE);
    for (unsigned i = 0; i < 40 && !IsZoomed(target.root()); ++i) Sleep(25);
    Check(target.WaitForClassicCaptionReflow(), "Drain the posted maximized reflow");
    Check(IsZoomed(target.root()) && client.Healthy(), "Maximizing keeps the caption attachment healthy");
    RECT maximized_strip{}, maximized_root{}, buttons{};
    MONITORINFO monitor{sizeof(monitor)};
    Check(GetMonitorInfoW(MonitorFromWindow(target.root(), MONITOR_DEFAULTTONEAREST), &monitor) != FALSE &&
        GetWindowRect(popup, &maximized_strip) && GetWindowRect(target.root(), &maximized_root) &&
        SUCCEEDED(DwmGetWindowAttribute(target.root(), DWMWA_CAPTION_BUTTON_BOUNDS, &buttons, sizeof(buttons))),
        "Read maximized fixture caption geometry");
    Check(IsWindowVisible(popup) && maximized_strip.top >= monitor.rcWork.top &&
        maximized_strip.right <= maximized_root.left + buttons.left,
        "Maximized caption stays onscreen and left of system buttons");
    ShowWindowAsync(target.root(), SW_MINIMIZE);
    for (unsigned i = 0; i < 40 && !IsIconic(target.root()); ++i) Sleep(25);
    Check(IsIconic(target.root()) && client.Healthy() && !IsWindowVisible(popup),
        "Minimizing hides the caption without detaching");
    ShowWindowAsync(target.root(), SW_RESTORE);
    for (unsigned i = 0; i < 40 && IsIconic(target.root()); ++i) Sleep(25);
    Check(target.WaitForClassicCaptionReflow(), "Drain the posted minimized-restore reflow");
    Check(!IsIconic(target.root()) && client.Healthy() && IsWindowVisible(popup),
        "Restoring from minimized recomputes and shows the caption");
    if (IsZoomed(target.root())) ShowWindowAsync(target.root(), SW_RESTORE);
    for (unsigned i = 0; i < 40 && IsZoomed(target.root()); ++i) Sleep(25);
    Check(target.WaitForClassicCaptionReflow(), "Drain the posted normal-restore reflow");
    Check(!IsIconic(target.root()) && !IsZoomed(target.root()) && client.Healthy() && IsWindowVisible(popup),
        "Restoring the maximized owner recomputes its normal caption");
    RECT restored_strip{}, restored_root{}, restored_buttons{};
    POINT restored_client{};
    Check(GetWindowRect(popup, &restored_strip) && GetWindowRect(target.root(), &restored_root) &&
        ClientToScreen(target.root(), &restored_client) &&
        SUCCEEDED(DwmGetWindowAttribute(target.root(), DWMWA_CAPTION_BUTTON_BOUNDS,
            &restored_buttons, sizeof(restored_buttons))), "Read restored caption geometry");
    Check(restored_strip.left >= restored_root.left && restored_strip.top >= restored_root.top &&
        restored_strip.right <= restored_root.left + restored_buttons.left &&
        restored_strip.bottom <= restored_client.y &&
        restored_strip.top - restored_root.top == moved.top - 48,
        "Restored caption returns to its owner's normal title band, not stale maximized bounds");
    WINDOWPLACEMENT placement{sizeof(placement)};
    Check(GetWindowPlacement(target.root(), &placement) != FALSE, "Snapshot classic fixture placement");
    Check(client.Present(placement, false) && !IsWindowVisible(popup), "Inactive owner cannot leave a floating caption");
    Check(client.Present(placement, true) && IsWindowVisible(popup), "Restored owner restores its caption");
    Check(client.Detach() && !IsWindow(popup) && IsWindowVisible(target.root()), "Detach destroys only the owned caption");
    quickky::HookClient recovery;
    Check(recovery.Prepare(sender, module, MakeIdentity(target)) && recovery.Commit(), "Reattach classic caption for recovery");
    const auto lost = OwnedStrip(target.root());
    Check(lost != nullptr, "Find only the synthetic caption for loss injection");
    SendMessageW(lost, WM_CLOSE, 0, 0);
    Check(!IsWindow(lost), "Destroy only the new synthetic caption surface");
    Sleep(1000);
    Check(IsWindowVisible(target.root()) && target.CreateExtraTab() != nullptr && !recovery.Healthy(),
        "Watchdog restores owner/creation policy after caption loss");
    target.DestroyExtraTab();
}

void TestDetachDuringNativeClip(HWND sender, HMODULE module)
{
    SyntheticTarget target;
    GuardOnlyProbe probe(target.root(), sender, target.thread_id(), module);
    auto& request = probe.request();
    request.workspace = 3;
    std::unique_ptr<void, decltype(&CloseHandle)> lease(
        CreateEventW(nullptr, TRUE, FALSE, quickky::LeaseEventName(request.nonce).c_str()), CloseHandle);
    Check(lease && probe.Attach().result == 1, "Prepare workspace for region reentrancy test");
    request.command = quickky::GuardCommand::Status;
    Check(probe.Send().result == 0x102, "Verify prepared status before region reentrancy test");
    request.command = quickky::GuardCommand::Commit;
    Check(probe.Send().result == 1, "Commit workspace before reentrant region update");
    Check(SendMessageW(target.root(), kDetachDuringClipMessage, reinterpret_cast<WPARAM>(sender),
        reinterpret_cast<LPARAM>(&request)) != 0, "Arm detach during SetWindowRgn");
    const HWND host = FindWindowExW(target.root(), nullptr, kHostClass, nullptr);
    WINDOWPOS position{host, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER};
    SendMessageW(host, WM_WINDOWPOSCHANGED, 0, reinterpret_cast<LPARAM>(&position));
    CheckNativeBand(target.root(), false);
}

} // namespace

int main(int argc, char* argv[])
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    try {
        const bool classic = argc == 2 && std::strcmp(argv[1], "--classic-caption") == 0;
        if (argc != 1 && !classic) return 160;
        if (classic) Check(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE,
            "Use physical caption coordinates for the interactive fixture");
        TestDirectoryBoundaryAndLocale();
        TestFolderAdmission();
        TestFolderSnapshots();
        RegisterSyntheticClasses();
        SyntheticTarget target;
        ControllerWindow controller;
        quickky::HookLibrary library;
        Check(library.Load(), "Load sibling QuickkyExplorerHook.dll");
        if (classic) {
            TestClassicCaption(controller.get(), library.module());
            std::cout << "PASS: classic caption fixture (not Explorer compatibility)\n";
            return 0;
        }

        const auto identity = MakeIdentity(target);
        TestGuardOnlyProtocol(target, controller.get(), library.module());
        TestWorkspaceBridge(target, controller.get(), library.module(), identity);
        TestLeaseWatchdog(target, controller.get(), library.module(), identity);
        TestRejectedCommit(target, controller.get(), library.module(), identity);
        TestAuthenticatedClose(controller.get(), library.module());
        TestDestructionDuringCommit(controller.get(), library.module());
        TestDestructionDuringResize(controller.get(), library.module());
        TestDestructionDuringConfigure(controller.get(), library.module());
        TestNativeHostLoss(controller.get(), library.module());
        TestDetachDuringNativeClip(controller.get(), library.module());
        TestUnsupportedNativeFrame(controller.get(), library.module());
        Check(!IsWindowVisible(target.root()) && !IsWindowVisible(target.initial_tab()),
            "Keep all synthetic Explorer windows hidden");
        std::cout << "PASS: Synthetic workspace bridge, recovery, boundary, and locale tests.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

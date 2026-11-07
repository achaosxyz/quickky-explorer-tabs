#include "hook/hook_client.h"
#include "runtime_files.h"

#include <utility>
#include <cstring>

namespace quickky {

HookLibrary::~HookLibrary()
{
    if (module_) FreeLibrary(module_);
    ClearFiles();
}

void HookLibrary::ClearFiles()
{
    if (!filename_.empty()) DeleteFileW(filename_.c_str());
    for (const auto& runtime : runtime_files_) DeleteFileW(runtime.c_str());
    if (!directory_.empty() && !RemoveDirectoryW(directory_.c_str())) {
        // Explorer can pin callbacks beyond controller exit; never force unload.
        OutputDebugStringA("Quickky: hook staging retained until loaded images are released\n");
    }
    filename_.clear();
    directory_.clear();
    runtime_files_.clear();
}

bool HookLibrary::Load()
{
    if (module_) return true;
    wchar_t executable[32768]{};
    const auto length = GetModuleFileNameW(nullptr, executable, 32768);
    wchar_t temporary[MAX_PATH + 1]{};
    const DWORD temporary_length = GetTempPathW(MAX_PATH + 1, temporary);
    GUID nonce{};
    wchar_t text[40]{};
    if (!length || length == 32768 || !temporary_length || temporary_length > MAX_PATH ||
        FAILED(CoCreateGuid(&nonce)) || !StringFromGUID2(nonce, text, 40)) return false;
    const std::wstring source_directory = std::wstring(executable).substr(0,
        std::wstring(executable).find_last_of(L"\\/") + 1);
    const std::wstring source = source_directory + L"QuickkyExplorerHook.dll";
    directory_ = std::wstring(temporary) + L"QTabs-" + text;
    if (!CreateDirectoryW(directory_.c_str(), nullptr)) { directory_.clear(); return false; }
    filename_ = directory_ + L"\\QuickkyExplorerHook.dll";
    for (const auto name : kRuntimeFiles) {
        const auto from = source_directory + name;
        if (GetFileAttributesW(from.c_str()) == INVALID_FILE_ATTRIBUTES &&
            GetLastError() == ERROR_FILE_NOT_FOUND) continue; // Installed-runtime developer build.
        const auto to = directory_ + L"\\" + name;
        if (!CopyFileW(from.c_str(), to.c_str(), TRUE)) { ClearFiles(); return false; }
        runtime_files_.push_back(to);
    }
    if (CopyFileW(source.c_str(), filename_.c_str(), TRUE)) {
        module_ = LoadLibraryExW(filename_.c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    }
    if (module_) return true;
    // The library survives failed enablement: release this attempt before retry
    // overwrites the only recorded staging paths. No target uses a failed image.
    ClearFiles();
    return false;
}

HookClient::~HookClient()
{
    Detach();
    if (exchange_) UnmapViewOfFile(exchange_);
    if (mapping_) CloseHandle(mapping_);
    if (completed_) CloseHandle(completed_);
    if (lease_) CloseHandle(lease_);
}

DWORD_PTR HookClient::Send(GuardRequest& request) const
{
    COPYDATASTRUCT data{kGuardProtocol, sizeof(request), &request};
    DWORD_PTR result = 0;
    const UINT flags = SMTO_ABORTIFHUNG | ((request.command == GuardCommand::Close ||
        request.command == GuardCommand::NativeRemove) ? 0 : SMTO_ERRORONEXIT);
    if (!target_ || !SendMessageTimeoutW(target_, WM_COPYDATA, reinterpret_cast<WPARAM>(sender_),
        reinterpret_cast<LPARAM>(&data), flags, 2000, &result)) return 0;
    return result;
}

bool HookClient::Prepare(HWND sender, HMODULE module, const WindowIdentity& window, WorkspaceMode mode)
{
    const bool native = mode == WorkspaceMode::Native;
    if (mode != WorkspaceMode::Classic && mode != WorkspaceMode::Native && mode != WorkspaceMode::Diagnostic) return false;
    if (target_ || !window.valid()) return false;
    DWORD pid = 0;
    const HWND target = reinterpret_cast<HWND>(window.window.handle);
    if (GetWindowThreadProcessId(target, &pid) != window.window.thread_id ||
        pid != window.window.process_id) return false;
    sender_ = sender;
    target_ = target;
    window_ = window;
    identity_.controller_pid = GetCurrentProcessId();
    identity_.target = window.window.handle;
    identity_.workspace = static_cast<std::uint32_t>(mode);
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user) ||
        FAILED(CoCreateGuid(&identity_.nonce))) return false;
    identity_.controller_started_at = (static_cast<std::uint64_t>(created.dwHighDateTime) << 32)
        | created.dwLowDateTime;
    const auto event_name = LeaseEventName(identity_.nonce);
    lease_ = CreateEventW(nullptr, TRUE, FALSE, event_name.c_str());
    if (!lease_ || GetLastError() == ERROR_ALREADY_EXISTS) return false;
    if (native) {
        completed_ = CreateEventW(nullptr, TRUE, FALSE, (NativeMappingName(identity_.nonce) + L".Ready").c_str());
        if (!completed_ || GetLastError() == ERROR_ALREADY_EXISTS) return false;
        mapping_ = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
            sizeof(NativeExchange), NativeMappingName(identity_.nonce).c_str());
        if (!mapping_ || GetLastError() == ERROR_ALREADY_EXISTS) return false;
        exchange_ = static_cast<NativeExchange*>(MapViewOfFile(mapping_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(NativeExchange)));
        if (!exchange_) return false;
    }
    const auto callback = reinterpret_cast<HOOKPROC>(GetProcAddress(module, "QuickkyBootstrap"));
    const auto hook = callback ? SetWindowsHookExW(WH_CALLWNDPROC, callback, module,
        window.window.thread_id) : nullptr;
    if (!hook) return false;
    const auto acknowledged = Send(identity_);
    UnhookWindowsHookEx(hook);
    auto status = identity_;
    status.command = GuardCommand::Status;
    const auto prepared = Send(status);
    if (!(acknowledged & 1) || (native ? ((prepared & 0xff) != 2 || !(prepared & 0xff00)) :
        (prepared & 0xffff) != 0x102)) { Cancel(); return false; }
    return true;
}

bool HookClient::Commit()
{
    auto request = identity_;
    request.command = GuardCommand::Commit;
    if (Send(request) != 1 || !Healthy()) { Cancel(); return false; }
    return true;
}

bool HookClient::RequestClose()
{
    auto request = identity_;
    request.command = GuardCommand::Close;
    return Send(request) == 1;
}

bool HookClient::Configure(const GuardRequest& tabs)
{
    auto request = tabs;
    request.size = sizeof(request);
    request.version = kGuardVersion;
    request.command = GuardCommand::Configure;
    request.controller_pid = identity_.controller_pid;
    request.controller_started_at = identity_.controller_started_at;
    request.target = identity_.target;
    request.nonce = identity_.nonce;
    return Send(request) == 1;
}

bool HookClient::Present(const WINDOWPLACEMENT& placement, bool visible)
{
    auto request = identity_;
    request.command = GuardCommand::Present;
    request.placement = placement;
    request.visible = visible ? 1 : 0;
    return Send(request) == 1;
}

bool HookClient::Healthy()
{
    auto request = identity_;
    request.command = GuardCommand::Status;
    const auto status = Send(request);
    return identity_.workspace == 2 ? (status & 0xff) == 1 && (status & 0xff00) != 0 :
        (status & 0xffff) == 0x101;
}

bool HookClient::Exchange(GuardCommand command, NativeExchange& packet)
{
    if (!exchange_ || !lease_ || WaitForSingleObject(lease_, 0) != WAIT_TIMEOUT) return false;
    packet.version = kGuardVersion;
    packet.request = ++sequence_;
    packet.state = 0;
    if (!ResetEvent(completed_)) return false;
    std::memcpy(exchange_, &packet, sizeof(packet));
    MemoryBarrier();
    auto request = identity_;
    request.command = command;
    request.input_tab = packet.request;
    const auto deadline = GetTickCount64() + 2000;
    bool completed = Send(request) == 1;
    while (completed && WaitForSingleObject(completed_, 0) != WAIT_OBJECT_0) {
        const auto now = GetTickCount64();
        if (now >= deadline) { completed = false; break; }
        const auto wait = MsgWaitForMultipleObjectsEx(1, &completed_, static_cast<DWORD>(deadline - now),
            QS_SENDMESSAGE, MWMO_INPUTAVAILABLE);
        if (wait == WAIT_OBJECT_0) break;
        if (wait != WAIT_OBJECT_0 + 1) { completed = false; break; }
        // Service synchronous COM/window callbacks without dispatching posted
        // timers or user commands that could destroy this controller mid-call.
        MSG message{};
        PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
    }
    if (!completed || InterlockedCompareExchange(&exchange_->state, 2, 2) != 2 ||
        exchange_->version != kGuardVersion || exchange_->request != packet.request) {
        Cancel(); // A late command must never share storage with another request.
        return false;
    }
    std::memcpy(&packet, exchange_, sizeof(packet));
    return SUCCEEDED(packet.result);
}

bool HookClient::ReadNative(std::vector<GUID>& tabs, GUID& active, OwnedPidl& folder)
{
    NativeExchange packet;
    if (!Exchange(GuardCommand::NativeSnapshot, packet) || !packet.count || packet.count > kMaxNativeTabs) return false;
    auto location = DescribeFolder(CloneFolderSnapshot(packet.folder, packet.folder_bytes));
    if (!location) return false;
    folder = std::move(location->pidl);
    tabs.assign(packet.tabs, packet.tabs + packet.count);
    active = packet.active;
    return true;
}

bool HookClient::AddNative(PCIDLIST_ABSOLUTE folder, GUID& created)
{
    NativeExchange packet;
    packet.folder_bytes = folder ? ILGetSize(folder) : 0;
    if (!packet.folder_bytes || packet.folder_bytes > sizeof(packet.folder)) return false;
    std::memcpy(packet.folder, folder, packet.folder_bytes);
    if (!Exchange(GuardCommand::NativeAdd, packet)) return false;
    created = packet.created;
    return !InlineIsEqualGUID(created, GUID{});
}

bool HookClient::RemoveNative(const GUID& tab, PCIDLIST_ABSOLUTE folder)
{
    NativeExchange packet;
    packet.active = tab;
    packet.folder_bytes = folder ? ILGetSize(folder) : 0;
    if (!packet.folder_bytes || packet.folder_bytes > sizeof(packet.folder)) return false;
    std::memcpy(packet.folder, folder, packet.folder_bytes);
    return Exchange(GuardCommand::NativeRemove, packet);
}

bool HookClient::AcceptInput(HWND sender, const GuardRequest& request) const
{
    return sender == target_ && request.command == GuardCommand::Input &&
        request.target == identity_.target && request.controller_pid == identity_.controller_pid &&
        request.controller_started_at == identity_.controller_started_at &&
        InlineIsEqualGUID(request.nonce, identity_.nonce);
}

void HookClient::Cancel() noexcept
{
    if (lease_) SetEvent(lease_);
}

bool HookClient::Detach()
{
    Cancel();
    if (!target_) return true;
    auto request = identity_;
    request.command = GuardCommand::Detach;
    const bool detached = Send(request) == 1;
    target_ = nullptr;
    return detached;
}

} // namespace quickky

#pragma once

#include "common/hook_protocol.h"
#include "common/window_identity.h"
#include "explorer/folder_location.h"
#include <vector>

namespace quickky {

// Owns a per-run hook/runtime copy so pinned callbacks never lock build output.
// Keep this library alive across workspace enable/disable cycles.
class HookLibrary {
public:
    HookLibrary() = default;
    HookLibrary(const HookLibrary&) = delete;
    HookLibrary& operator=(const HookLibrary&) = delete;
    ~HookLibrary();
    bool Load();
    HMODULE module() const noexcept { return module_; }
private:
    void ClearFiles();
    HMODULE module_ = nullptr;
    std::wstring directory_;
    std::wstring filename_;
    std::vector<std::wstring> runtime_files_;
};

// Controller-thread owner of one nonce-bound attachment and cancellation lease.
// Prepare binds lifetime/watchdog only, without changing chrome, tabs or visibility.
// Commit starts integration; Present subsequently applies workspace visibility.
// Cancellation precedes detach so a late response cannot strand a hidden root.
// Native failure means unconfirmed, not rolled back: an already-started native
// call can complete after the timeout cancels its lease.
class HookClient {
public:
    HookClient() = default;
    HookClient(const HookClient&) = delete;
    HookClient& operator=(const HookClient&) = delete;
    ~HookClient();
    bool Prepare(HWND sender, HMODULE module, const WindowIdentity& window,
        WorkspaceMode mode = WorkspaceMode::Classic);
    bool Commit();
    bool RequestClose();
    bool Configure(const GuardRequest& tabs);
    bool Present(const WINDOWPLACEMENT& placement, bool visible);
    bool Healthy();
    bool ReadNative(std::vector<GUID>& tabs, GUID& active, OwnedPidl& folder);
    bool AddNative(PCIDLIST_ABSOLUTE folder, GUID& created);
    bool RemoveNative(const GUID& tab, PCIDLIST_ABSOLUTE folder);
    bool AcceptInput(HWND sender, const GuardRequest& request) const;
    void Cancel() noexcept;
    bool Detach();
    const WindowIdentity& window() const noexcept { return window_; }
private:
    DWORD_PTR Send(GuardRequest& request) const;
    bool Exchange(GuardCommand command, NativeExchange& packet);
    HWND sender_ = nullptr;
    HWND target_ = nullptr;
    HANDLE lease_ = nullptr;
    HANDLE mapping_ = nullptr;
    HANDLE completed_ = nullptr;
    NativeExchange* exchange_ = nullptr;
    std::uint64_t sequence_ = 0;
    GuardRequest identity_;
    WindowIdentity window_;
};

} // namespace quickky

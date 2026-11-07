#pragma once

#include "capture/capture_coordinator.h"
#include "capture/explorer_window_probe.h"
#include "hook/hook_client.h"
#include "explorer/folder_location.h"

#include <deque>
#include <map>
#include <memory>

namespace quickky {

// Controller-STA policy owner: admits acknowledged roots and publishes one view.
// Input is queued during synchronous IPC, then processed after the call unwinds.
// Stop cancels every lease before restoring any root; it never closes Explorer.
class WorkspaceController {
public:
    WorkspaceController(HWND sender, CaptureCoordinator& capture, GroupModel& group,
        ExplorerWindowProbe& probe, HookLibrary& library, std::wstring root);
    ~WorkspaceController();
    bool Initialize();
    bool Refresh(const std::map<NativeWindow, WindowCandidate>& visible);
    bool QueueInput(WPARAM sender, LPARAM packet);
    bool ProcessInput();
    void Stop();

private:
    struct Entry { std::unique_ptr<HookClient> client; std::wstring label; };
    struct Input { WindowIdentity source; TabInput action; TabId tab; OwnedPidl folder; };
    bool Publish();
    bool RefreshNative(const std::map<NativeWindow, WindowCandidate>& visible);
    bool InNativeScope(HWND target) const;
    void SavePlacement();
    void Release(NativeWindow window);
    const Tab* FindTab(TabId id) const;

    HWND sender_;
    CaptureCoordinator& capture_;
    GroupModel& group_;
    ExplorerWindowProbe& probe_;
    std::wstring root_;
    HookLibrary& library_;
    std::map<NativeWindow, Entry> entries_;
    std::deque<Input> inputs_;
    WINDOWPLACEMENT placement_{sizeof(WINDOWPLACEMENT)};
    bool placed_ = false;
    bool native_ = false;
    NativeWindow main_ = 0;
    std::map<NativeWindow, WindowCandidate> quarantined_;
};

} // namespace quickky

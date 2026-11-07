#pragma once

#include "capture/capture_coordinator.h"
#include "capture/explorer_window_probe.h"
#include "hook/hook_client.h"
#include "explorer/folder_location.h"

#include <deque>
#include <cwchar>
#include <map>
#include <memory>

namespace quickky {

// Acknowledged custom publication state, scoped to one attachment lifetime.
class WorkspacePublicationState {
public:
    bool NeedsConfigure(const GuardRequest& view) const
    {
        if (!configured_ || view.count != view_.count || view.active != view_.active) return true;
        for (unsigned i = 0; i < view.count; ++i) {
            if (view.tabs[i].id != view_.tabs[i].id ||
                std::wcscmp(view.tabs[i].title, view_.tabs[i].title) != 0) return true;
        }
        return false;
    }
    void Configured(const GuardRequest& view) { view_ = view; configured_ = true; }
    bool NeedsPresent(const WINDOWPLACEMENT& placement, bool visible, bool actual_visible) const
    {
        if (!presented_ || visible_ != visible || visible != actual_visible) return true;
        // Hidden members receive the current placement when selected, not on every move.
        return visible && (placement.flags != placement_.flags || placement.showCmd != placement_.showCmd ||
            placement.ptMinPosition.x != placement_.ptMinPosition.x ||
            placement.ptMinPosition.y != placement_.ptMinPosition.y ||
            placement.ptMaxPosition.x != placement_.ptMaxPosition.x ||
            placement.ptMaxPosition.y != placement_.ptMaxPosition.y ||
            !EqualRect(&placement.rcNormalPosition, &placement_.rcNormalPosition));
    }
    void Presented(const WINDOWPLACEMENT& placement, bool visible)
    { placement_ = placement; visible_ = visible; presented_ = true; }

private:
    GuardRequest view_;
    WINDOWPLACEMENT placement_{sizeof(WINDOWPLACEMENT)};
    bool configured_ = false;
    bool presented_ = false;
    bool visible_ = false;
};

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
    // Scheduling hint for the last pass, never a claim about an uncertain move.
    bool native_transfer_completed() const noexcept { return native_transferred_; }
    bool QueueInput(WPARAM sender, LPARAM packet);
    bool ProcessInput();
    void Stop();

private:
    friend struct WorkspaceControllerTestAccess;
    struct Entry {
        std::unique_ptr<HookClient> client;
        std::wstring label;
        WorkspacePublicationState publication;
    };
    struct Input { WindowIdentity source; TabInput action; TabId tab; OwnedPidl folder; };
    bool Publish();
    bool RefreshNative(const std::map<NativeWindow, WindowCandidate>& visible);
    std::vector<HWND> FolderRoots(const std::map<NativeWindow, WindowCandidate>& visible) const;
    std::map<NativeWindow, WindowIdentity> SnapshotAttachments() const;
    void ReconcileEntries(const FolderLocations& locations,
        const std::map<NativeWindow, WindowIdentity>& attachments);
    bool InNativeScope(HWND target) const;
    bool InNativeScope(HWND target, const FolderLocations& locations) const;
    void SavePlacement();
    void Release(NativeWindow window);
    void ReleaseIfCurrent(NativeWindow window, const WindowIdentity& identity);
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
    bool native_transferred_ = false;
    NativeWindow main_ = 0;
    std::map<NativeWindow, WindowCandidate> quarantined_;
};

} // namespace quickky

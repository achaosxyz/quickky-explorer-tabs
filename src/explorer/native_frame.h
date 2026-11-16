#pragma once

#include "explorer/folder_location.h"
#include "explorer/folder_selection.h"
#include "common/hook_protocol.h"
#include "explorer/native_abi.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace quickky {

// Owning-UI-thread adapter for a validated ExplorerFrame accessor contract. It never
// hides windows or draws chrome. A COM reference keeps the native object alive
// through reentrant window destruction; callbacks retain this adapter as well.
class NativeFrame : public std::enable_shared_from_this<NativeFrame> {
public:
    static std::shared_ptr<NativeFrame> Bind(HWND root);
    ~NativeFrame();
    bool Snapshot(std::vector<GUID>& tabs, GUID& active, FolderLocation& folder);
    HRESULT CaptureSelection(std::vector<GUID>& tabs, GUID& active,
        FolderLocation& folder, FolderSelection& selection);
    HRESULT RestoreSelection(const GUID& tab, PCIDLIST_ABSOLUTE folder,
        const FolderSelection& selection, bool& applied, bool& folder_ready,
        const std::function<bool()>& unchanged);
    HRESULT Add(PCIDLIST_ABSOLUTE folder, GUID& created);
    HRESULT Remove(const GUID& tab, PCIDLIST_ABSOLUTE expected_folder,
        const FolderSelection& expected_selection, const std::function<bool()>& unchanged);
    // Post-transfer activation/file-view focus in every window state. It never
    // adds, removes or retries a tab, and cannot override later destination input.
    HRESULT Reveal(const GUID& tab, const std::function<bool()>& unchanged);
    bool InstallPlus(std::weak_ptr<void> owner, std::function<bool()> available);
    void StopPlus();

private:
    friend struct NativeFrameTestAccess;
    NativeFrame() = default;
    bool Valid() const;
    bool ReadTabs(std::vector<GUID>& tabs, GUID& active) const;
    HRESULT ActiveView(IFolderView** view) const;
    HRESULT AddOriginal(PCIDLIST_ABSOLUTE folder, int entry, GUID* created);
    HRESULT AddAtOriginal(PCIDLIST_ABSOLUTE folder, int entry, int index, GUID* created);
    static HRESULT STDMETHODCALLTYPE Plus(void* self, PCIDLIST_ABSOLUTE folder, int entry, GUID* created);
    static HRESULT STDMETHODCALLTYPE PlusAt(void* self, PCIDLIST_ABSOLUTE folder, int entry, int index, GUID* created);
    HWND root_ = nullptr;
    DWORD thread_ = 0;
    unsigned char* object_ = nullptr;
    void* actions_ = nullptr;
    void** original_ = nullptr;
    NativeAbiLayout layout_;
    std::array<void*, 13> proxy_{};
    std::weak_ptr<void> owner_;
    std::function<bool()> available_;
    bool intercepting_ = false;
};

} // namespace quickky

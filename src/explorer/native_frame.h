#pragma once

#include "explorer/folder_location.h"
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
    HRESULT Add(PCIDLIST_ABSOLUTE folder, GUID& created);
    HRESULT Remove(const GUID& tab, PCIDLIST_ABSOLUTE expected_folder);
    bool InstallPlus(std::weak_ptr<void> owner, std::function<bool()> available);
    void StopPlus();

private:
    NativeFrame() = default;
    bool Valid() const;
    bool ReadTabs(std::vector<GUID>& tabs, GUID& active) const;
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

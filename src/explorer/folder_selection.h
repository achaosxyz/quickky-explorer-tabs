#pragma once

#include "explorer/folder_location.h"
#include <functional>
#include <shobjidl.h>
#include <cstdint>

namespace quickky {

constexpr unsigned kMaxSelectionItems = 256;
constexpr unsigned kMaxSelectionBytes = 64 * 1024;

// PIDLs are relative children owned by the corresponding folder view, not paths.
// Focus may belong to an unselected item; absence does not require clearing focus.
struct FolderSelection {
    std::vector<OwnedPidl> selected;
    OwnedPidl focused;
};

bool SameSelection(const FolderSelection& left, const FolderSelection& right);
bool SelectionRestored(const FolderSelection& expected, const FolderSelection& actual);
bool EncodeSelection(const FolderSelection& selection, void* bytes, std::size_t capacity,
    std::uint32_t& length);
bool DecodeSelection(const void* bytes, std::size_t length, FolderSelection& selection);
HRESULT ReadSelection(IFolderView* view, PCIDLIST_ABSOLUTE folder, FolderSelection& selection);
// Apply once, then only verify on later UI ticks. Revalidation runs before writes
// and after COM callouts, which may pump cancellation/navigation messages.
HRESULT RestoreSelection(IFolderView* view, PCIDLIST_ABSOLUTE folder,
    const FolderSelection& selection, bool& applied, const std::function<bool()>& available);

// Empty startup views need a grace period for asynchronous /select launches.
// This is bounded observation, not a guarantee about a future external request.
class SelectionStability {
public:
    bool Observe(ULONGLONG now, bool same, bool empty);
private:
    ULONGLONG started_ = 0;
    ULONGLONG changed_ = 0;
    bool sampled_ = false;
};

} // namespace quickky

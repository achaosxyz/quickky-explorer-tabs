#include "explorer/folder_selection.h"

#include <algorithm>
#include <cstring>
#include <wrl/client.h>

namespace quickky {
namespace {
using Microsoft::WRL::ComPtr;

bool SameItem(PCUIDLIST_RELATIVE left, PCUIDLIST_RELATIVE right)
{
    return left && right ? ILIsEqual(left, right) != FALSE : left == right;
}

bool SameItems(const FolderSelection& left, const FolderSelection& right)
{
    return left.selected.size() == right.selected.size() &&
        std::all_of(left.selected.begin(), left.selected.end(), [&right](const OwnedPidl& item) {
            return std::any_of(right.selected.begin(), right.selected.end(), [&item](const OwnedPidl& other) {
                return SameItem(item.get(), other.get());
            });
        });
}

HRESULT CheckFolder(IFolderView* view, PCIDLIST_ABSOLUTE expected)
{
    if (!view || !expected) return E_INVALIDARG;
    ComPtr<IPersistFolder2> persist;
    if (FAILED(view->GetFolder(IID_PPV_ARGS(&persist)))) return E_PENDING;
    PIDLIST_ABSOLUTE current = nullptr;
    const auto result = persist->GetCurFolder(&current);
    OwnedPidl owned(current);
    return SUCCEEDED(result) && owned && ILIsEqual(owned.get(), expected) ? S_OK : E_PENDING;
}

std::vector<PCUITEMID_CHILD> Children(const FolderSelection& selection)
{
    std::vector<PCUITEMID_CHILD> children;
    for (const auto& item : selection.selected) children.push_back(item.get());
    return children;
}
} // namespace

bool SameSelection(const FolderSelection& left, const FolderSelection& right)
{
    return SameItems(left, right) && SameItem(left.focused.get(), right.focused.get());
}

bool SelectionRestored(const FolderSelection& expected, const FolderSelection& actual)
{
    return SameItems(expected, actual) &&
        (!expected.focused || SameItem(expected.focused.get(), actual.focused.get()));
}

bool EncodeSelection(const FolderSelection& selection, void* bytes, std::size_t capacity,
    std::uint32_t& length)
{
    length = 0;
    if (!bytes || selection.selected.size() > kMaxSelectionItems) return false;
    capacity = std::min<std::size_t>(capacity, kMaxSelectionBytes);
    auto output = static_cast<unsigned char*>(bytes);
    std::size_t offset = 0;
    const auto put = [&](const void* value, std::size_t size) {
        if (size > capacity - offset) return false;
        std::memcpy(output + offset, value, size);
        offset += size;
        return true;
    };
    const auto count = static_cast<std::uint32_t>(selection.selected.size());
    if (!put(&count, sizeof(count))) return false;
    const auto item = [&](PCUIDLIST_RELATIVE pidl) {
        const std::uint32_t size = pidl ? ILGetSize(pidl) : 0;
        return (!pidl || (size > sizeof(USHORT) && size <= kMaxSelectionBytes)) &&
            put(&size, sizeof(size)) && (!size || put(pidl, size));
    };
    for (const auto& selected : selection.selected) if (!selected || !item(selected.get())) return false;
    if (!item(selection.focused.get())) return false;
    length = static_cast<std::uint32_t>(offset);
    return true;
}

bool DecodeSelection(const void* bytes, std::size_t length, FolderSelection& selection)
{
    selection = {};
    if (!bytes || length > kMaxSelectionBytes) return false;
    const auto input = static_cast<const unsigned char*>(bytes);
    std::size_t offset = 0;
    const auto get = [&](void* value, std::size_t size) {
        if (size > length - offset) return false;
        std::memcpy(value, input + offset, size);
        offset += size;
        return true;
    };
    std::uint32_t count = 0;
    if (!get(&count, sizeof(count)) || count > kMaxSelectionItems) return false;
    FolderSelection decoded;
    const auto item = [&](OwnedPidl& pidl, bool optional) {
        std::uint32_t size = 0;
        if (!get(&size, sizeof(size))) return false;
        if (!size) return optional;
        if (size <= sizeof(USHORT) || size > length - offset) return false;
        pidl = CloneFolderSnapshot(input + offset, size);
        offset += size;
        return pidl != nullptr;
    };
    for (unsigned i = 0; i < count; ++i) {
        OwnedPidl selected;
        if (!item(selected, false) || std::any_of(decoded.selected.begin(), decoded.selected.end(),
            [&selected](const OwnedPidl& other) { return SameItem(selected.get(), other.get()); })) return false;
        decoded.selected.push_back(std::move(selected));
    }
    if (!item(decoded.focused, true) || offset != length) return false;
    selection = std::move(decoded);
    return true;
}

HRESULT ReadSelection(IFolderView* view, PCIDLIST_ABSOLUTE folder, FolderSelection& selection)
{
    selection = {};
    if (CheckFolder(view, folder) != S_OK) return E_PENDING;
    int count = 0;
    if (FAILED(view->ItemCount(SVGIO_SELECTION, &count))) return E_PENDING;
    if (count < 0 || count > static_cast<int>(kMaxSelectionItems)) return HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
    FolderSelection captured;
    if (count) {
        ComPtr<IEnumIDList> items;
        if (FAILED(view->Items(SVGIO_SELECTION, IID_PPV_ARGS(&items)))) return E_PENDING;
        for (int i = 0; i < count; ++i) {
            PITEMID_CHILD child = nullptr;
            const auto next = items->Next(1, &child, nullptr);
            OwnedPidl owned(child);
            if (next != S_OK || !owned) return E_PENDING;
            captured.selected.push_back(std::move(owned));
        }
    }
    int focused = -1;
    if (FAILED(view->GetFocusedItem(&focused))) return E_PENDING;
    if (focused >= 0) {
        PITEMID_CHILD child = nullptr;
        const auto result = view->Item(focused, &child);
        captured.focused.reset(child);
        if (FAILED(result) || !captured.focused) return E_PENDING;
    }
    int after = 0;
    if (FAILED(view->ItemCount(SVGIO_SELECTION, &after)) || count != after ||
        CheckFolder(view, folder) != S_OK) return E_PENDING;
    selection = std::move(captured);
    return S_OK;
}

HRESULT RestoreSelection(IFolderView* view, PCIDLIST_ABSOLUTE folder,
    const FolderSelection& selection, bool& applied, const std::function<bool()>& available)
{
    if (!available || !available()) return E_ABORT;
    FolderSelection current;
    const auto read = ReadSelection(view, folder, current);
    if (!available()) return E_ABORT;
    if (read != S_OK) return read;
    if (SelectionRestored(selection, current)) return S_OK;
    if (applied) return E_PENDING; // Never repeat writes after accepting an apply.
    // A folder PIDL can be published before its contents. Confirm each requested
    // child is recognized by the new view before making any selection writes.
    ComPtr<IFolderView2> ready;
    if (FAILED(view->QueryInterface(IID_PPV_ARGS(&ready)))) return E_NOINTERFACE;
    const auto present = [&](PCUITEMID_CHILD child) {
        DWORD flags = 0;
        const auto result = ready->GetSelectionState(child, &flags);
        return SUCCEEDED(result);
    };
    for (const auto& child : selection.selected) {
        if (!present(child.get())) return E_PENDING;
        if (!available()) return E_ABORT;
    }
    if (selection.focused && !present(selection.focused.get())) return E_PENDING;
    if (!available()) return E_ABORT;
    auto old_items = Children(current);
    auto new_items = Children(selection);
    // Mark before the first write: reentrancy/failure is not permission to retry.
    applied = true;
    if (!old_items.empty()) {
        if (!available() || CheckFolder(view, folder) != S_OK || !available()) return E_ABORT;
        const auto result = view->SelectAndPositionItems(static_cast<UINT>(old_items.size()),
            old_items.data(), nullptr, SVSI_DESELECT | SVSI_NOTAKEFOCUS);
        if (FAILED(result) || !available()) return FAILED(result) ? result : E_ABORT;
    }
    if (!new_items.empty()) {
        if (!available() || CheckFolder(view, folder) != S_OK || !available()) return E_ABORT;
        const auto result = view->SelectAndPositionItems(static_cast<UINT>(new_items.size()),
            new_items.data(), nullptr, SVSI_SELECT | SVSI_NOTAKEFOCUS);
        if (FAILED(result) || !available()) return FAILED(result) ? result : E_ABORT;
    }
    if (selection.focused) {
        PCUITEMID_CHILD focused = selection.focused.get();
        if (!available() || CheckFolder(view, folder) != S_OK || !available()) return E_ABORT;
        DWORD flags = SVSI_FOCUSED | SVSI_ENSUREVISIBLE | SVSI_NOTAKEFOCUS;
        if (std::any_of(selection.selected.begin(), selection.selected.end(), [&selection](const OwnedPidl& item) {
            return SameItem(item.get(), selection.focused.get());
        })) flags |= SVSI_SELECT;
        const auto result = view->SelectAndPositionItems(1, &focused, nullptr, flags);
        if (FAILED(result) || !available()) return FAILED(result) ? result : E_ABORT;
    }
    FolderSelection confirmed;
    const auto result = ReadSelection(view, folder, confirmed);
    if (!available()) return E_ABORT;
    if (result != S_OK) return result;
    return SelectionRestored(selection, confirmed) ? S_OK : E_PENDING;
}

bool SelectionStability::Observe(ULONGLONG now, bool same, bool empty)
{
    if (!sampled_) { sampled_ = true; started_ = changed_ = now; }
    if (!same) changed_ = now;
    return now - started_ >= (empty ? 200u : 50u) && now - changed_ >= 50;
}

} // namespace quickky

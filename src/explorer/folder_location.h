#pragma once

#include <windows.h>
#include <shlobj.h>

#include <memory>
#include <map>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace quickky {

struct ShellMemoryDeleter {
    void operator()(void* value) const noexcept { CoTaskMemFree(value); }
};

struct ShellPidlDeleter {
    using pointer = PIDLIST_ABSOLUTE;
    void operator()(pointer value) const noexcept { CoTaskMemFree(reinterpret_cast<void*>(value)); }
};

using OwnedPidl = std::unique_ptr<ITEMIDLIST, ShellPidlDeleter>;
constexpr std::size_t kMaxFolderSnapshotBytes = 64 * 1024;

struct FolderLocation {
    OwnedPidl pidl;
    std::wstring parsing_name;
    std::wstring display_name;
};

using FolderLocations = std::map<HWND, std::vector<FolderLocation>>;
// One read-only automation pass; an unreadable/excluded view empties its root.
FolderLocations ResolveFolderLocations(const std::vector<HWND>& roots);
std::optional<FolderLocation> ResolveFolder(HWND root);
// A native frame can have several automation entries; scope checks inspect all.
std::vector<FolderLocation> ResolveFolders(HWND root);
OwnedPidl CloneFolderSnapshot(const void* bytes, std::size_t length);
// Shared admission for real folders and virtual browsing roots, excluding
// Control Panel/management namespaces regardless of localized window captions.
std::optional<FolderLocation> DescribeFolder(OwnedPidl pidl);
bool OpenFolderWindow(const FolderLocation& folder);
std::optional<std::wstring> NormalizeDirectory(const std::wstring& path);
bool IsWithinDirectory(const std::wstring& path, const std::wstring& root);

} // namespace quickky

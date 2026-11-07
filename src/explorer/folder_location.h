#pragma once

#include <windows.h>
#include <shlobj.h>

#include <memory>
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

std::optional<FolderLocation> ResolveFolder(HWND root);
// A native frame can have several automation entries; scope checks inspect all.
std::vector<FolderLocation> ResolveFolders(HWND root);
OwnedPidl CloneFolderSnapshot(const void* bytes, std::size_t length);
std::optional<FolderLocation> DescribeFolder(OwnedPidl pidl);
bool OpenFolderWindow(const FolderLocation& folder);
std::optional<std::wstring> NormalizeDirectory(const std::wstring& path);
bool IsWithinDirectory(const std::wstring& path, const std::wstring& root);

} // namespace quickky

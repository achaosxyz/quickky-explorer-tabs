#include "explorer/folder_location.h"

#include <exdisp.h>
#include <shellapi.h>
#include <servprov.h>
#include <shobjidl.h>
#include <shlguid.h>
#include <wrl/client.h>

#include <cwchar>
#include <cstring>
#include <cstdint>
#include <utility>
#include <vector>

namespace quickky {
namespace {
using Microsoft::WRL::ComPtr;

bool IsManagementNamespace(const std::wstring& parsing_name)
{
    constexpr const wchar_t* roots[] = {
        L"{26EE0668-A00A-44D7-9371-BEB064C98683}", // Control Panel categories
        L"{21EC2020-3AEA-1069-A2DD-08002B30309D}", // Classic Control Panel
        L"{ED7BA470-8E54-465E-825C-99712043E01C}", // All Control Panel tasks
        L"{7007ACC7-3202-11D1-AAD2-00805FC1270E}", // Network Connections
        L"{2227A280-3AEA-1069-A2DE-08002B30309D}"  // Printers
    };
    for (std::size_t offset = parsing_name.find(L'{'); offset != std::wstring::npos;
         offset = parsing_name.find(L'{', offset + 1)) {
        // Canonical ::{CLSID} components and folder.{CLSID} junctions identify
        // namespaces independently of localized captions or ordinary folder names.
        const bool component = offset >= 2 && parsing_name.compare(offset - 2, 2, L"::") == 0 &&
            (offset == 2 || parsing_name[offset - 3] == L'\\');
        const bool junction = offset && parsing_name[offset - 1] == L'.';
        if ((!component && !junction) || parsing_name.size() - offset < 38 ||
            parsing_name[offset + 37] != L'}' || (parsing_name.size() > offset + 38 &&
                parsing_name[offset + 38] != L'\\')) continue;
        auto name = parsing_name.substr(offset, 38);
        GUID id{};
        if (FAILED(CLSIDFromString(name.data(), &id))) continue;
        for (const auto root : roots) if (_wcsicmp(name.c_str(), root) == 0) return true;
        // Directly addressed applets need not retain a Control Panel parent PIDL.
        const auto key = L"CLSID\\" + name;
        DWORD bytes = 0;
        if (RegGetValueW(HKEY_CLASSES_ROOT, key.c_str(), L"System.ControlPanel.Category",
            RRF_RT_ANY, nullptr, nullptr, &bytes) == ERROR_SUCCESS) return true;
    }
    return false;
}

std::optional<FolderLocation> ReadFolder(IWebBrowserApp* browser)
{
    ComPtr<IServiceProvider> services;
    ComPtr<IShellBrowser> shell;
    ComPtr<IShellView> view;
    ComPtr<IFolderView> folders;
    ComPtr<IPersistFolder2> persist;
    if (FAILED(browser->QueryInterface(IID_PPV_ARGS(&services))) ||
        FAILED(services->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&shell))) ||
        FAILED(shell->QueryActiveShellView(&view)) ||
        FAILED(view.As(&folders)) || FAILED(folders->GetFolder(IID_PPV_ARGS(&persist)))) {
        return std::nullopt;
    }
    PIDLIST_ABSOLUTE pidl = nullptr;
    if (FAILED(persist->GetCurFolder(&pidl)) || !pidl) return std::nullopt;
    return DescribeFolder(OwnedPidl(pidl));
}
} // namespace

std::optional<FolderLocation> DescribeFolder(OwnedPidl pidl)
{
    if (!pidl) return std::nullopt;
    ComPtr<IShellItem> item;
    SFGAOF attributes = 0;
    if (FAILED(SHCreateItemFromIDList(pidl.get(), IID_PPV_ARGS(&item))) ||
        FAILED(item->GetAttributes(SFGAO_FOLDER, &attributes)) || !(attributes & SFGAO_FOLDER)) {
        return std::nullopt;
    }
    FolderLocation result;
    result.pidl = std::move(pidl);
    PWSTR parsing = nullptr;
    if (FAILED(SHGetNameFromIDList(result.pidl.get(), SIGDN_DESKTOPABSOLUTEPARSING, &parsing))) {
        return std::nullopt;
    }
    const std::unique_ptr<wchar_t, ShellMemoryDeleter> parsing_owner(parsing);
    result.parsing_name = parsing;
    if (IsManagementNamespace(result.parsing_name)) return std::nullopt;
    PWSTR display = nullptr;
    if (SUCCEEDED(SHGetNameFromIDList(result.pidl.get(), SIGDN_NORMALDISPLAY, &display))) {
        const std::unique_ptr<wchar_t, ShellMemoryDeleter> display_owner(display);
        result.display_name = display;
    }
    if (result.display_name.empty()) result.display_name = result.parsing_name;
    return result;
}

OwnedPidl CloneFolderSnapshot(const void* bytes, std::size_t length)
{
    if (!bytes || length < sizeof(std::uint16_t) || length > kMaxFolderSnapshotBytes) return {};
    const auto data = static_cast<const unsigned char*>(bytes);
    std::size_t offset = 0;
    // Never let ILCloneFull walk an unchecked or unterminated cross-process list.
    while (length - offset >= sizeof(std::uint16_t)) {
        std::uint16_t size = 0;
        std::memcpy(&size, data + offset, sizeof(size));
        if (!size) return offset + sizeof(size) == length ?
            OwnedPidl(ILCloneFull(reinterpret_cast<PCIDLIST_ABSOLUTE>(data))) : OwnedPidl{};
        if (size < sizeof(size) || size > length - offset) return {};
        offset += size;
    }
    return {};
}

std::vector<FolderLocation> ResolveFolders(HWND root)
{
    std::vector<FolderLocation> result;
    ComPtr<IShellWindows> windows;
    if (FAILED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER,
                               IID_PPV_ARGS(&windows)))) return result;
    long count = 0;
    if (FAILED(windows->get_Count(&count))) return result;
    for (long index = 0; index < count; ++index) {
        VARIANT item{};
        item.vt = VT_I4;
        item.lVal = index;
        ComPtr<IDispatch> dispatch;
        ComPtr<IWebBrowserApp> browser;
        SHANDLE_PTR handle = 0;
        if (FAILED(windows->Item(item, &dispatch)) || !dispatch ||
            FAILED(dispatch.As(&browser)) || FAILED(browser->get_HWND(&handle)) ||
            reinterpret_cast<HWND>(handle) != root) continue;
        auto folder = ReadFolder(browser.Get());
        if (folder) result.push_back(std::move(*folder));
        else return {};
    }
    return result;
}

std::optional<FolderLocation> ResolveFolder(HWND root)
{
    auto folders = ResolveFolders(root);
    if (folders.size() != 1) return std::nullopt;
    return std::move(folders.front());
}

bool OpenFolderWindow(const FolderLocation& folder)
{
    if (!folder.pidl) return false;
    SHELLEXECUTEINFOW launch{sizeof(launch)};
    launch.fMask = SEE_MASK_IDLIST | SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
    launch.lpVerb = L"opennewwindow";
    launch.lpIDList = folder.pidl.get();
    launch.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&launch) != FALSE;
}

std::optional<std::wstring> NormalizeDirectory(const std::wstring& path)
{
    const DWORD length = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (!length) return std::nullopt;
    std::vector<wchar_t> buffer(length);
    const DWORD copied = GetFullPathNameW(path.c_str(), length, buffer.data(), nullptr);
    if (!copied || copied >= length) return std::nullopt;
    std::wstring result(buffer.data(), copied);
    const DWORD attributes = GetFileAttributesW(result.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return std::nullopt;
    }
    while (result.size() > 3 && result.back() == L'\\') result.pop_back();
    return result;
}

bool IsWithinDirectory(const std::wstring& path, const std::wstring& root)
{
    if (root.empty() || path.size() < root.size() ||
        _wcsnicmp(path.c_str(), root.c_str(), root.size()) != 0) return false;
    return path.size() == root.size() || root.back() == L'\\' || path[root.size()] == L'\\';
}

} // namespace quickky

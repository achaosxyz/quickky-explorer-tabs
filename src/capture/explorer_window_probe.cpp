#include "capture/explorer_window_probe.h"

#include <dwmapi.h>

#include <array>
#include <cwchar>
#include <memory>

namespace quickky {
namespace {

using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;

bool ReadToken(HANDLE token, TOKEN_INFORMATION_CLASS information,
    std::vector<unsigned char>& buffer)
{
    DWORD length = 0;
    GetTokenInformation(token, information, nullptr, 0, &length);
    if (length == 0 || GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
        return false;
    }
    buffer.resize(length);
    return GetTokenInformation(token, information, buffer.data(), length, &length) != FALSE;
}

std::optional<DWORD> Integrity(HANDLE token)
{
    std::vector<unsigned char> buffer;
    if (!ReadToken(token, TokenIntegrityLevel, buffer)) {
        return std::nullopt;
    }
    const auto label = reinterpret_cast<const TOKEN_MANDATORY_LABEL*>(buffer.data());
    if (!IsValidSid(label->Label.Sid)) {
        return std::nullopt;
    }
    const auto count = *GetSidSubAuthorityCount(label->Label.Sid);
    if (count == 0) {
        return std::nullopt;
    }
    return *GetSidSubAuthority(label->Label.Sid, count - 1);
}

} // namespace

bool ExplorerWindowProbe::Initialize()
{
    ready_ = false;
    HANDLE raw_token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &raw_token)) {
        return false;
    }
    const Handle token(raw_token, CloseHandle);
    const auto integrity = Integrity(token.get());
    if (!integrity || !ReadToken(token.get(), TokenUser, user_)
        || !ProcessIdToSessionId(GetCurrentProcessId(), &session_)) {
        return false;
    }
    integrity_ = *integrity;

    std::array<wchar_t, MAX_PATH> directory{};
    const auto length = GetWindowsDirectoryW(directory.data(), static_cast<UINT>(directory.size()));
    if (length == 0 || length >= directory.size()) {
        return false;
    }
    explorer_path_.assign(directory.data(), length);
    explorer_path_ += L"\\explorer.exe";

    desktops_.Reset();
    if (FAILED(CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(desktops_.GetAddressOf())))) {
        return false;
    }
    ready_ = true;
    return true;
}

std::optional<WindowCandidate> ExplorerWindowProbe::Inspect(HWND window, bool include_hidden) const
{
    if (!ready_ || window == nullptr || (!include_hidden && !IsWindowVisible(window))
        || GetAncestor(window, GA_ROOT) != window || GetWindow(window, GW_OWNER) != nullptr) {
        return std::nullopt;
    }
    std::array<wchar_t, 64> class_name{};
    if (GetClassNameW(window, class_name.data(), static_cast<int>(class_name.size())) == 0
        || std::wcscmp(class_name.data(), L"CabinetWClass") != 0) {
        return std::nullopt;
    }

    DWORD cloaked = 0;
    BOOL current_desktop = FALSE;
    if (FAILED(DwmGetWindowAttribute(window, DWMWA_CLOAKED, &cloaked, sizeof(cloaked)))
        || cloaked != 0
        || FAILED(desktops_->IsWindowOnCurrentVirtualDesktop(window, &current_desktop))
        || !current_desktop) {
        return std::nullopt;
    }

    DWORD process_id = 0;
    const auto thread_id = GetWindowThreadProcessId(window, &process_id);
    DWORD session = 0;
    if (thread_id == 0 || !ProcessIdToSessionId(process_id, &session) || session != session_) {
        return std::nullopt;
    }
    const Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id), CloseHandle);
    if (!process) {
        return std::nullopt;
    }
    std::wstring path(32768, L'\0');
    auto length = static_cast<DWORD>(path.size());
    if (!QueryFullProcessImageNameW(process.get(), 0, path.data(), &length)) {
        return std::nullopt;
    }
    path.resize(length);
    if (_wcsicmp(path.c_str(), explorer_path_.c_str()) != 0) {
        return std::nullopt;
    }

    HANDLE raw_token = nullptr;
    if (!OpenProcessToken(process.get(), TOKEN_QUERY, &raw_token)) {
        return std::nullopt;
    }
    const Handle token(raw_token, CloseHandle);
    std::vector<unsigned char> user;
    const auto integrity = Integrity(token.get());
    if (!integrity || *integrity != integrity_ || !ReadToken(token.get(), TokenUser, user)
        || !EqualSid(reinterpret_cast<const TOKEN_USER*>(user.data())->User.Sid,
            reinterpret_cast<const TOKEN_USER*>(user_.data())->User.Sid)) {
        return std::nullopt;
    }

    FILETIME created{}, exited{}, kernel{}, cpu{};
    if (!GetProcessTimes(process.get(), &created, &exited, &kernel, &cpu)) {
        return std::nullopt;
    }
    DWORD current_process = 0;
    if (GetWindowThreadProcessId(window, &current_process) != thread_id
        || current_process != process_id) {
        return std::nullopt;
    }

    const auto started = (static_cast<std::uint64_t>(created.dwHighDateTime) << 32)
        | created.dwLowDateTime;
    return WindowCandidate{reinterpret_cast<NativeWindow>(window), process_id, thread_id, started};
}

} // namespace quickky

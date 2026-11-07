#include "settings/preferences.h"

#include <optional>
#include <vector>

namespace quickky {
namespace {
constexpr wchar_t kPreferences[] = L"Software\\QTabs";
constexpr wchar_t kRun[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kStartupValue[] = L"QTabs";

std::optional<std::wstring> ReadString(const wchar_t* key, const wchar_t* name)
{
    DWORD bytes = 0;
    if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ, nullptr, nullptr, &bytes) !=
        ERROR_SUCCESS || bytes < sizeof(wchar_t) || bytes > 65536 || bytes % sizeof(wchar_t)) {
        return std::nullopt;
    }
    std::vector<wchar_t> value(bytes / sizeof(wchar_t));
    if (RegGetValueW(HKEY_CURRENT_USER, key, name, RRF_RT_REG_SZ, nullptr, value.data(), &bytes) !=
        ERROR_SUCCESS) return std::nullopt;
    return std::wstring(value.data());
}
}

std::wstring ExecutablePath()
{
    std::wstring path(32768, L'\0');
    const auto size = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (!size || size == path.size()) return {};
    path.resize(size);
    return path;
}

std::wstring ResolveLocale(const std::wstring& preference, LANGID system_language)
{
    if (preference == L"en-US" || preference == L"zh-CN") return preference;
    return PRIMARYLANGID(system_language) == LANG_CHINESE ? L"zh-CN" : L"en-US";
}

const TrayStrings& CurrentTrayStrings()
{
    const auto preference = ReadString(kPreferences, L"Language").value_or(L"");
    return ResolveLocale(preference, GetUserDefaultUILanguage()) == L"zh-CN" ? kChinese : kEnglish;
}

bool StartupEnabled()
{
    const auto value = ReadString(kRun, kStartupValue);
    const auto path = ExecutablePath();
    return value && !path.empty() && _wcsicmp(value->c_str(), (L"\"" + path + L"\"").c_str()) == 0;
}

bool SetStartupEnabled(bool enabled)
{
    const auto path = ExecutablePath();
    if (path.empty()) return false;
    const auto command = L"\"" + path + L"\"";
    if (enabled && command.size() > 260) return false;
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRun, 0, nullptr, 0, KEY_SET_VALUE, nullptr,
        &key, nullptr) != ERROR_SUCCESS) return false;
    const auto status = enabled ? RegSetValueExW(key, kStartupValue, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(command.c_str()),
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t))) : RegDeleteValueW(key, kStartupValue);
    RegCloseKey(key);
    return status == ERROR_SUCCESS || (!enabled && status == ERROR_FILE_NOT_FOUND);
}

bool TabsEnabled()
{
    DWORD enabled = 0;
    DWORD bytes = sizeof(enabled);
    return RegGetValueW(HKEY_CURRENT_USER, kPreferences, L"TabsEnabled", RRF_RT_REG_DWORD,
        nullptr, &enabled, &bytes) == ERROR_SUCCESS && enabled != 0;
}

bool SetTabsEnabled(bool enabled)
{
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kPreferences, 0, nullptr, 0, KEY_SET_VALUE, nullptr,
        &key, nullptr) != ERROR_SUCCESS) return false;
    const DWORD value = enabled ? 1 : 0;
    const auto status = RegSetValueExW(key, L"TabsEnabled", 0, REG_DWORD,
        reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(key);
    return status == ERROR_SUCCESS;
}

} // namespace quickky

#pragma once

#include "tray_strings.h"
#include <windows.h>
#include <string>

namespace quickky {

std::wstring ExecutablePath();
std::wstring ResolveLocale(const std::wstring& preference, LANGID system_language);
const TrayStrings& CurrentTrayStrings();
bool StartupEnabled();
bool SetStartupEnabled(bool enabled);
bool TabsEnabled();
bool SetTabsEnabled(bool enabled);

} // namespace quickky

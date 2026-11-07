#pragma once

#include <windows.h>
#include <shellapi.h>

namespace quickky {

constexpr UINT kTrayMenuMessage = WM_APP + 20;
enum class TrayAction : UINT { None = 0, Startup = 1001, Tabs, About, Exit };

// Controller-thread owner of the tray broker and its localized modal popups.
// CancelPopups lets a stop request unwind a menu/About nested message loop.
class TrayIcon {
public:
    TrayIcon() = default;
    TrayIcon(const TrayIcon&) = delete;
    TrayIcon& operator=(const TrayIcon&) = delete;
    ~TrayIcon();
    bool Create(HINSTANCE instance, HWND controller);
    TrayAction ShowMenu(bool tabs_enabled);
    void ShowAbout();
    void CancelPopups();
private:
    static HRESULT CALLBACK AboutProcedure(HWND, UINT, WPARAM, LPARAM, LONG_PTR);
    static LRESULT CALLBACK Procedure(HWND, UINT, WPARAM, LPARAM);
    bool Add();
    HWND window_ = nullptr;
    HWND controller_ = nullptr;
    HWND about_ = nullptr;
    HICON icon_ = nullptr;
    NOTIFYICONDATAW data_{};
    UINT taskbar_created_ = 0;
};

} // namespace quickky

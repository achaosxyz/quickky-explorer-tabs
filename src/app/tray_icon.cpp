#include "app/tray_icon.h"
#include "settings/preferences.h"
#include "build_metadata.h"
#include "resource.h"

#include <commctrl.h>
#include <string>

namespace quickky {
namespace {
constexpr UINT kCallback = WM_APP + 21;
constexpr wchar_t kGithub[] = L"https://github.com/achaosxyz/quickky-explorer-tabs";
HRESULT CALLBACK AboutCallback(HWND, UINT message, WPARAM, LPARAM parameter, LONG_PTR)
{
    if (message == TDN_HYPERLINK_CLICKED &&
        wcscmp(reinterpret_cast<const wchar_t*>(parameter), kGithub) == 0) {
        ShellExecuteW(nullptr, L"open", kGithub, nullptr, nullptr, SW_SHOWNORMAL);
    }
    return S_OK;
}
}

TrayIcon::~TrayIcon()
{
    if (data_.hWnd) Shell_NotifyIconW(NIM_DELETE, &data_);
    if (window_) DestroyWindow(window_);
    if (icon_) DestroyIcon(icon_);
}

bool TrayIcon::Create(HINSTANCE instance, HWND controller)
{
    controller_ = controller;
    taskbar_created_ = RegisterWindowMessageW(L"TaskbarCreated");
    icon_ = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(IDI_QUICKKY),
        IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    if (!icon_) return false;
    WNDCLASSW definition{};
    definition.lpfnWndProc = Procedure;
    definition.hInstance = instance;
    definition.lpszClassName = L"QuickkyExplorerTabs.Tray";
    if (!RegisterClassW(&definition) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    // A hidden top-level broker receives TaskbarCreated broadcasts; message-only windows do not.
    window_ = CreateWindowExW(WS_EX_TOOLWINDOW, definition.lpszClassName, L"", WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, instance, this);
    return window_ && Add();
}

bool TrayIcon::Add()
{
    data_.cbSize = sizeof(data_);
    data_.hWnd = window_;
    data_.uID = 1;
    data_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
    data_.uCallbackMessage = kCallback;
    data_.hIcon = icon_;
    wcscpy_s(data_.szTip, L"Quickky Explorer Tabs");
    if (!Shell_NotifyIconW(NIM_ADD, &data_)) return false;
    data_.uVersion = NOTIFYICON_VERSION_4;
    return Shell_NotifyIconW(NIM_SETVERSION, &data_) != FALSE;
}

LRESULT CALLBACK TrayIcon::Procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto self = reinterpret_cast<TrayIcon*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<TrayIcon*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self && message == self->taskbar_created_) { self->Add(); return 0; }
    if (self && message == kCallback && (LOWORD(lparam) == WM_CONTEXTMENU ||
        LOWORD(lparam) == NIN_KEYSELECT || LOWORD(lparam) == NIN_SELECT)) {
        PostMessageW(self->controller_, kTrayMenuMessage, 0, 0);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

TrayAction TrayIcon::ShowMenu(bool tabs_enabled)
{
    const auto& text = CurrentTrayStrings();
    const auto menu = CreatePopupMenu();
    if (!menu) return TrayAction::None;
    AppendMenuW(menu, MF_STRING | (StartupEnabled() ? MF_CHECKED : MF_UNCHECKED),
        static_cast<UINT>(TrayAction::Startup), text.startup);
    AppendMenuW(menu, MF_STRING | (tabs_enabled ? MF_CHECKED : MF_UNCHECKED),
        static_cast<UINT>(TrayAction::Tabs), text.enable_tabs);
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(TrayAction::About), text.about);
    AppendMenuW(menu, MF_STRING, static_cast<UINT>(TrayAction::Exit), text.exit);
    POINT position{};
    GetCursorPos(&position);
    SetForegroundWindow(window_);
    const auto command = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
        position.x, position.y, window_, nullptr);
    PostMessageW(window_, WM_NULL, 0, 0);
    DestroyMenu(menu);
    return static_cast<TrayAction>(command);
}

void TrayIcon::ShowAbout()
{
    const auto& text = CurrentTrayStrings();
    const std::wstring content = std::wstring(text.author) + L": aCHAOS\nachaosxyz@gmail.com\n\n" +
        text.version + L": " + QUICKKY_VERSION + L"\n" + text.build_time + L": " +
        QUICKKY_BUILD_TIME + L"\n\n<a href=\"" + kGithub + L"\">GitHub</a>";
    TASKDIALOGCONFIG dialog{sizeof(dialog)};
    dialog.hwndParent = window_;
    dialog.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_USE_HICON_MAIN | TDF_ALLOW_DIALOG_CANCELLATION;
    dialog.dwCommonButtons = TDCBF_CLOSE_BUTTON;
    dialog.pszWindowTitle = text.about_title;
    dialog.pszMainInstruction = L"Quickky Explorer Tabs";
    dialog.pszContent = content.c_str();
    dialog.hMainIcon = icon_;
    dialog.pfCallback = AboutProcedure;
    dialog.lpCallbackData = reinterpret_cast<LONG_PTR>(this);
    TaskDialogIndirect(&dialog, nullptr, nullptr, nullptr);
}

HRESULT CALLBACK TrayIcon::AboutProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                         LONG_PTR data)
{
    auto* self = reinterpret_cast<TrayIcon*>(data);
    if (message == TDN_CREATED) self->about_ = window;
    if (message == TDN_DESTROYED) self->about_ = nullptr;
    return AboutCallback(window, message, wparam, lparam, data);
}

void TrayIcon::CancelPopups()
{
    EndMenu();
    if (about_) SendMessageW(about_, TDM_CLICK_BUTTON, IDCLOSE, 0);
}

} // namespace quickky

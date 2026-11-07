#include "explorer/native_tab_guard.h"

#include <cwchar>
#include <commctrl.h>
#include <utility>

namespace quickky {
namespace {

bool IsShellTab(HWND window)
{
    wchar_t name[128]{};
    return GetClassNameW(window, name, 128) && wcscmp(name, L"ShellTabWindowClass") == 0;
}

BOOL CALLBACK CountChild(HWND child, LPARAM parameter)
{
    if (IsShellTab(child)) ++*reinterpret_cast<unsigned*>(parameter);
    return TRUE;
}

} // namespace

thread_local NativeTabGuard* NativeTabGuard::current_ = nullptr;

unsigned NativeTabGuard::CountTabs(HWND target)
{
    unsigned count = 0;
    EnumChildWindows(target, CountChild, reinterpret_cast<LPARAM>(&count));
    return count;
}

bool NativeTabGuard::Start(HWND target)
{
    if (current_ || CountTabs(target) != 1 ||
        GetWindowThreadProcessId(target, nullptr) != GetCurrentThreadId()) return false;
    target_ = target;
    current_ = this;
    hook_ = SetWindowsHookExW(WH_CBT, CreateHook, nullptr, GetCurrentThreadId());
    if (!hook_) { current_ = nullptr; target_ = nullptr; return false; }
    return true;
}

void NativeTabGuard::Stop()
{
    RestoreBar();
    if (hook_) UnhookWindowsHookEx(hook_);
    hook_ = nullptr;
    target_ = nullptr;
    if (current_ == this) current_ = nullptr;
}

bool NativeTabGuard::SuppressBar(std::weak_ptr<void> owner, std::function<void()> lost)
{
    if (!hook_ || bar_ || owner.expired()) return false;
    constexpr auto name = L"Microsoft.UI.Content.DesktopChildSiteBridge";
    const auto host = FindWindowExW(target_, nullptr, name, nullptr);
    if (!host || FindWindowExW(target_, host, name, nullptr) ||
        GetWindowThreadProcessId(host, nullptr) != GetCurrentThreadId()) return false;
    auto original = CreateRectRgn(0, 0, 0, 0);
    if (!original) return false;
    if (GetWindowRgn(host, original) == ERROR) { DeleteObject(original); original = nullptr; }
    if (!SetWindowSubclass(host, BarProcedure, 1, reinterpret_cast<DWORD_PTR>(this))) {
        if (original) DeleteObject(original);
        return false;
    }
    bar_ = host;
    original_region_ = original;
    owner_ = std::move(owner);
    lost_ = std::move(lost);
    if (ApplyBarClip()) return true;
    RestoreBar();
    return false;
}

bool NativeTabGuard::ApplyBarClip()
{
    RECT client{}, bounds{};
    POINT origin{};
    if (!bar_ || (GetWindowLongPtrW(bar_, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) ||
        !GetClientRect(target_, &client) || !ClientToScreen(target_, &origin) ||
        !GetWindowRect(bar_, &bounds) || bounds.left != origin.x || bounds.top != origin.y ||
        bounds.right - bounds.left != client.right) return false;
    const auto scale = GetDpiForWindow(target_) / 96.0f;
    const int width = client.right - static_cast<int>(150 * scale);
    const int height = static_cast<int>(40 * scale);
    if (width <= 0 || height <= 0 || bounds.bottom - bounds.top < height) return false;
    auto region = CreateRectRgn(0, 0, bounds.right - bounds.left, bounds.bottom - bounds.top);
    auto cut = CreateRectRgn(0, 0, width, height);
    if (!region || !cut) {
        if (region) DeleteObject(region);
        if (cut) DeleteObject(cut);
        return false;
    }
    bool valid = !original_region_ || CombineRgn(region, region, original_region_, RGN_AND) != ERROR;
    valid = valid && CombineRgn(region, region, cut, RGN_DIFF) != ERROR;
    DeleteObject(cut);
    // SetWindowRgn synchronously sends window-position messages. Suppress only
    // our own recursive update; the callback keepalive protects reentrant detach.
    updating_ = true;
    const bool applied = valid && SetWindowRgn(bar_, region, TRUE);
    updating_ = false;
    if (!applied) DeleteObject(region); // Success transfers ownership to Windows.
    if (restore_pending_) {
        restore_pending_ = false;
        RestoreBar(); // The outer SetWindowRgn must finish before restoration.
        return false;
    }
    return applied && bar_ != nullptr;
}

void NativeTabGuard::RestoreBar()
{
    if (updating_) { restore_pending_ = true; return; }
    const auto host = std::exchange(bar_, nullptr);
    const auto original = std::exchange(original_region_, nullptr);
    DWORD_PTR binding = 0;
    const bool bound = host && GetWindowSubclass(host, BarProcedure, 1, &binding) &&
        binding == reinterpret_cast<DWORD_PTR>(this);
    if (bound) RemoveWindowSubclass(host, BarProcedure, 1);
    if ((!bound || !SetWindowRgn(host, original, TRUE)) && original) DeleteObject(original);
    owner_.reset();
    lost_ = {};
}

LRESULT CALLBACK NativeTabGuard::BarProcedure(HWND window, UINT message, WPARAM wparam,
    LPARAM lparam, UINT_PTR, DWORD_PTR binding)
{
    auto* self = reinterpret_cast<NativeTabGuard*>(binding);
    const auto lifetime = self->owner_.lock();
    if (!lifetime) return DefSubclassProc(window, message, wparam, lparam);
    if (message == WM_NCDESTROY) {
        self->bar_ = nullptr; // Never restore a destroyed/reused host HWND.
        RemoveWindowSubclass(window, BarProcedure, 1);
        const auto lost = std::move(self->lost_);
        if (lost) lost();
    }
    const auto result = DefSubclassProc(window, message, wparam, lparam);
    if (self->bar_ == window && !self->updating_ &&
        (message == WM_WINDOWPOSCHANGED || message == WM_DPICHANGED || message == WM_STYLECHANGED) &&
        !self->ApplyBarClip()) {
        const auto lost = std::move(self->lost_);
        if (lost) lost();
    }
    return result;
}

LRESULT CALLBACK NativeTabGuard::CreateHook(int code, WPARAM wparam, LPARAM lparam)
{
    if (code == HCBT_CREATEWND && current_) {
        const auto created = reinterpret_cast<HWND>(wparam);
        const auto details = reinterpret_cast<const CBT_CREATEWNDW*>(lparam);
        const HWND parent = details->lpcs->hwndParent;
        if (parent && GetAncestor(parent, GA_ROOT) == current_->target_ &&
            IsShellTab(created) && CountTabs(current_->target_) >= 1) {
            ++current_->blocked_;
            return 1;
        }
    }
    return CallNextHookEx(nullptr, code, wparam, lparam);
}

} // namespace quickky

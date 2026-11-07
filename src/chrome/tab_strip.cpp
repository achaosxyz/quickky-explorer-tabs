#include "chrome/tab_strip.h"

#include <windowsx.h>
#include <dwmapi.h>
#include <algorithm>
#include <utility>

namespace quickky {
namespace {
constexpr wchar_t kClass[] = L"Quickky.TabStrip";
bool UsesRoundedTabs()
{
    // Windows 11 starts at build 22000. This selects appearance only; it is not
    // an Explorer integration capability check. The host must be version-aware.
    OSVERSIONINFOEXW version{sizeof(version)};
    version.dwMajorVersion = 10;
    version.dwBuildNumber = 22000;
    DWORDLONG conditions = 0;
    for (const DWORD field : {VER_MAJORVERSION, VER_MINORVERSION, VER_BUILDNUMBER}) {
        conditions = VerSetConditionMask(conditions, field, VER_GREATER_EQUAL);
    }
    return VerifyVersionInfoW(&version, VER_MAJORVERSION | VER_MINORVERSION | VER_BUILDNUMBER,
        conditions) != FALSE;
}
D2D1_COLOR_F Rgb(unsigned value)
{
    return D2D1::ColorF((value >> 16) / 255.0f, ((value >> 8) & 255) / 255.0f,
                        (value & 255) / 255.0f);
}

// Explorer paints Quick Access controls inside its extended caption. Ask the
// owner's hit testing instead of guessing their width or covering Ribbon UI.
// Root callers retain the attachment across these reentrant window messages.
bool CaptionPoint(HWND root, int x, int y)
{
    POINT client{x, y};
    if (!ScreenToClient(root, &client)) return false;
    const auto child = ChildWindowFromPointEx(root, client, CWP_SKIPINVISIBLE);
    return (!child || child == root) &&
        SendMessageW(root, WM_NCHITTEST, 0, MAKELPARAM(x, y)) == HTCAPTION;
}

bool CaptionColumn(HWND root, int x, int top, int bottom)
{
    return CaptionPoint(root, x, top) && CaptionPoint(root, x, top + (bottom - top) / 2) &&
        CaptionPoint(root, x, bottom - 1);
}

bool RibbonBelowCaption(HWND root, int bottom)
{
    for (HWND dock = nullptr; (dock = FindWindowExW(root, dock, L"UIRibbonCommandBarDock", nullptr)) != nullptr;) {
        RECT bounds{};
        if (IsWindowVisible(dock) && GetWindowRect(dock, &bounds) &&
            bounds.right > bounds.left && bounds.bottom > bounds.top && bounds.top >= bottom) return true;
    }
    return false;
}
}

TabStrip::~TabStrip()
{
    if (window_ && GetWindowLongPtrW(window_, GWLP_USERDATA) == reinterpret_cast<LONG_PTR>(this)) {
        DestroyWindow(window_);
    }
}

bool TabStrip::Create(HWND parent, Input input, std::weak_ptr<void> lifetime, bool classic_caption)
{
    parent_ = parent;
    input_ = std::move(input);
    lifetime_ = std::move(lifetime);
    classic_caption_ = classic_caption;
    corner_radius_ = !classic_caption_ && UsesRoundedTabs() ? 7.0f : 0.0f;
    if (classic_caption_ && (!IsWindowVisible(parent_) || IsIconic(parent_) || !IsWindowEnabled(parent_))) return false;
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&Procedure), &module)) return false;
    WNDCLASSW definition{};
    definition.lpfnWndProc = Procedure;
    definition.hInstance = module;
    definition.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    definition.lpszClassName = kClass;
    definition.style = CS_DBLCLKS;
    if (!RegisterClassW(&definition) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.GetAddressOf())) ||
        FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(text_factory_.GetAddressOf())))) return false;
    window_ = CreateWindowExW(classic_caption_ ? WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE : 0,
        kClass, L"Quickky Explorer Tabs", classic_caption_ ? WS_POPUP : WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, parent, nullptr, module, this);
    if (!window_) return false;
    if (classic_caption_) {
        auto previous = GetWindow(parent_, GW_HWNDPREV);
        if (previous == window_) previous = GetWindow(window_, GW_HWNDPREV);
        if (previous && (GetWindowLongPtrW(previous, GWL_EXSTYLE) & WS_EX_TOPMOST)) previous = nullptr;
        // Join the owner's band; creating a caption for a background owner
        // must not raise it over unrelated applications or move the owner.
        if (!SetWindowPos(window_, previous ? previous : HWND_TOP, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER)) return false;
    }
    return Resize() && Resources() && format_;
}

bool TabStrip::Configure(const GuardRequest& request)
{
    if (request.count > kMaxWorkspaceTabs) return false;
    for (unsigned i = 0; i < request.count; ++i) {
        if (!wmemchr(request.tabs[i].title, L'\0', 96)) return false;
    }
    const bool selection_changed = state_.active != request.active || state_.count != request.count;
    state_ = request;
    const auto title = std::wstring(L"Quickky Explorer Tabs (") + std::to_wstring(state_.count) + L")";
    SetWindowTextW(window_, title.c_str());
    Layout();
    for (unsigned i = 0; i < state_.count; ++i) {
        if (!selection_changed || state_.tabs[i].id != state_.active) continue;
        if (i < first_) first_ = i;
        if (shown_ && i >= first_ + shown_) first_ = i - shown_ + 1;
    }
    Layout();
    InvalidateRect(window_, nullptr, FALSE);
    return true;
}

void TabStrip::Hide()
{
    if (window_) ShowWindow(window_, SW_HIDE);
}

bool TabStrip::Resize()
{
    if (!window_) return false;
    if (resizing_) return true;
    resizing_ = true;
    struct Reset { bool& value; ~Reset() { value = false; } } reset{resizing_};
    const auto window = window_;
    RECT bounds{};
    if (!GetClientRect(parent_, &bounds)) return false;
    scale_ = GetDpiForWindow(parent_) / 96.0f;
    height_ = static_cast<int>(40 * scale_);
    int left = 0, top = 0;
    int width = std::max(0, static_cast<int>(bounds.right) - static_cast<int>(150 * scale_));
    if (classic_caption_) {
        if (!IsWindowVisible(parent_) || IsIconic(parent_) || !IsWindowEnabled(parent_)) { Hide(); return true; }
        RECT root{}, buttons{};
        if ((GetWindowLongPtrW(parent_, GWL_EXSTYLE) & WS_EX_LAYOUTRTL) ||
            !(GetWindowLongPtrW(parent_, GWL_STYLE) & WS_CAPTION) ||
            !GetWindowRect(parent_, &root)) { OutputDebugStringA("Quickky: caption root information unavailable\n"); return false; }
        MONITORINFO monitor{sizeof(monitor)};
        RECT visible{};
        if (!GetMonitorInfoW(MonitorFromWindow(parent_, MONITOR_DEFAULTTONEAREST), &monitor)) {
            OutputDebugStringA("Quickky: caption monitor information unavailable\n"); return false;
        }
        // During restore the minimized style can clear before the offscreen
        // minimized placement changes. Do not consume DWM geometry in that gap.
        if (!IntersectRect(&visible, &root, &monitor.rcMonitor)) { Hide(); return true; }
        if (FAILED(DwmGetWindowAttribute(parent_, DWMWA_CAPTION_BUTTON_BOUNDS, &buttons, sizeof(buttons)))) {
            OutputDebugStringA("Quickky: caption buttons unavailable\n"); return false;
        }
        // DWM reports window-relative buttons, including the offscreen outer
        // frame when maximized. Preserve that frame and all native controls.
        const UINT dpi = GetDpiForWindow(parent_);
        const int frame = GetSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
            GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        left = root.left + static_cast<LONG>(32 * scale_);
        top = root.top + std::max<LONG>(frame, buttons.top);
        int bottom = root.top + buttons.bottom;
        POINT client_origin{};
        if (!ClientToScreen(parent_, &client_origin) ||
            (bottom > client_origin.y && !RibbonBelowCaption(parent_, bottom))) {
            OutputDebugStringA("Quickky: caption Ribbon layout unavailable\n"); return false;
        }
        int right = root.left + buttons.left;
        if (bottom <= top || right > root.right || bottom > root.bottom) {
            OutputDebugStringA("Quickky: caption button bounds invalid\n"); return false;
        }
        // Keep the rightmost clear caption span. Quick Access buttons report
        // non-caption hits, including after toolbar customization or maximize.
        const int middle = top + (bottom - top) / 2;
        while (right > left && !CaptionPoint(parent_, right - 1, middle)) --right;
        // Ribbon's resize hit region can extend past the system-metric seed.
        // Trim to native caption rows rather than cover that resize border.
        while (top < bottom && !CaptionPoint(parent_, right - 1, top)) ++top;
        while (bottom > top && !CaptionPoint(parent_, right - 1, bottom - 1)) --bottom;
        int clear_left = right;
        while (clear_left > left && CaptionColumn(parent_, clear_left - 1, top, bottom)) --clear_left;
        left = clear_left;
        width = right - left;
        height_ = bottom - top;
        if (width < 140 * scale_ || height_ < 16 * scale_ || height_ > 64 * scale_ ||
            root.left + buttons.right > root.right || top + height_ > root.bottom) {
            char trace[256]{};
            wsprintfA(trace, "Quickky: caption span rejected root=%ld,%ld,%ld,%ld buttons=%ld,%ld,%ld,%ld span=%d,%d,%d,%d\n",
                root.left, root.top, root.right, root.bottom, buttons.left, buttons.top, buttons.right, buttons.bottom,
                left, top, width, height_);
            OutputDebugStringA(trace);
            return false;
        }
    }
    const UINT flags = SWP_NOACTIVATE | (classic_caption_ ? SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_SHOWWINDOW : 0);
    if (!SetWindowPos(window, HWND_TOP, left, top, width, height_, flags) || window_ != window) return false;
    if (target_) target_->Resize(D2D1::SizeU(width, height_));
    format_.Reset();
    text_factory_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 12 * scale_, L"en-US", &format_);
    if (format_) {
        format_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
        format_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        Microsoft::WRL::ComPtr<IDWriteInlineObject> ellipsis;
        if (SUCCEEDED(text_factory_->CreateEllipsisTrimmingSign(format_.Get(), &ellipsis))) {
            DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            format_->SetTrimming(&trimming, ellipsis.Get());
        }
    }
    Layout();
    InvalidateRect(window_, nullptr, FALSE);
    return true;
}

void TabStrip::Layout()
{
    RECT bounds{};
    GetClientRect(window_, &bounds);
    const float available = std::max(0.0f, bounds.right - 44 * scale_);
    tab_width_ = std::min(200 * scale_, std::max(92 * scale_,
        available / std::max(1u, state_.count)));
    const unsigned capacity = static_cast<unsigned>(available / tab_width_);
    shown_ = std::min(state_.count, capacity);
    first_ = std::min(first_, state_.count - shown_);
}

TabStrip::Hit TabStrip::HitTest(int x, int y) const
{
    if (x < 0 || y < 0 || y >= height_) return {TabInput::Select, 0};
    const unsigned index = static_cast<unsigned>(x / tab_width_);
    if (index < shown_) {
        const bool close = x >= (index + 1) * tab_width_ - 28 * scale_;
        return {close ? TabInput::Close : TabInput::Select, state_.tabs[first_ + index].id};
    }
    const float plus = shown_ * tab_width_;
    if (x >= plus && x < plus + 40 * scale_) return {TabInput::New, 0};
    return {TabInput::Select, 0};
}

bool TabStrip::Resources()
{
    if (target_ && brush_) return true;
    brush_.Reset();
    target_.Reset();
    RECT bounds{};
    GetClientRect(window_, &bounds);
    if (FAILED(factory_->CreateHwndRenderTarget(D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(window_, D2D1::SizeU(bounds.right, bounds.bottom)),
        &target_))) return false;
    target_->SetDpi(96, 96);
    return SUCCEEDED(target_->CreateSolidColorBrush(Rgb(0), &brush_));
}

void TabStrip::Paint()
{
    PAINTSTRUCT paint{};
    BeginPaint(window_, &paint);
    if (Resources() && format_) {
        DWORD light = 1;
        DWORD bytes = sizeof(light);
        RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &bytes);
        HIGHCONTRASTW contrast{sizeof(contrast)};
        SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
        const bool high_contrast = (contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
        const auto background = high_contrast ? D2D1::ColorF(
            GetRValue(GetSysColor(COLOR_WINDOW)) / 255.0f,
            GetGValue(GetSysColor(COLOR_WINDOW)) / 255.0f,
            GetBValue(GetSysColor(COLOR_WINDOW)) / 255.0f) : Rgb(light ? 0xf0f0f0 : 0x202020);
        const auto foreground = high_contrast ? D2D1::ColorF(
            GetRValue(GetSysColor(COLOR_WINDOWTEXT)) / 255.0f,
            GetGValue(GetSysColor(COLOR_WINDOWTEXT)) / 255.0f,
            GetBValue(GetSysColor(COLOR_WINDOWTEXT)) / 255.0f) : Rgb(light ? 0x202020 : 0xf0f0f0);
        target_->BeginDraw();
        target_->Clear(background);
        const auto hovered = HitTest(hover_x_, hover_y_);
        for (unsigned i = 0; i < shown_; ++i) {
            const auto& tab = state_.tabs[first_ + i];
            const float left = i * tab_width_;
            const bool selected = tab.id == state_.active;
            brush_->SetColor(high_contrast ? background : Rgb(selected ?
                (light ? 0xffffff : 0x323232) : (hovered.tab == tab.id ?
                (light ? 0xe3e3e3 : 0x292929) : (light ? 0xf0f0f0 : 0x202020))));
            const auto shape = D2D1::RoundedRect(D2D1::RectF(left + 2 * scale_, 4 * scale_,
                left + tab_width_ - 2 * scale_, static_cast<float>(height_)),
                corner_radius_ * scale_, corner_radius_ * scale_);
            target_->FillRoundedRectangle(shape, brush_.Get());
            brush_->SetColor(foreground);
            target_->DrawTextW(tab.title, static_cast<UINT32>(wcslen(tab.title)), format_.Get(),
                D2D1::RectF(left + 14 * scale_, 4 * scale_, left + tab_width_ - 34 * scale_,
                    static_cast<float>(height_)), brush_.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
            const float x = left + tab_width_ - 17 * scale_;
            const float y = height_ * 0.5f;
            target_->DrawLine(D2D1::Point2F(x - 3 * scale_, y - 3 * scale_),
                D2D1::Point2F(x + 3 * scale_, y + 3 * scale_), brush_.Get(), scale_);
            target_->DrawLine(D2D1::Point2F(x + 3 * scale_, y - 3 * scale_),
                D2D1::Point2F(x - 3 * scale_, y + 3 * scale_), brush_.Get(), scale_);
            if (selected) target_->DrawLine(D2D1::Point2F(left + 12 * scale_, height_ - scale_),
                D2D1::Point2F(left + tab_width_ - 12 * scale_, height_ - scale_), brush_.Get(), 2 * scale_);
        }
        // High-contrast glyphs must not fall back to the independent app theme.
        brush_->SetColor(foreground);
        const float plus = shown_ * tab_width_ + 20 * scale_;
        const float middle = height_ * 0.5f;
        target_->DrawLine(D2D1::Point2F(plus - 5 * scale_, middle),
            D2D1::Point2F(plus + 5 * scale_, middle), brush_.Get(), scale_);
        target_->DrawLine(D2D1::Point2F(plus, middle - 5 * scale_),
            D2D1::Point2F(plus, middle + 5 * scale_), brush_.Get(), scale_);
        if (target_->EndDraw() == D2DERR_RECREATE_TARGET) { brush_.Reset(); target_.Reset(); }
    }
    EndPaint(window_, &paint);
}

LRESULT CALLBACK TabStrip::Procedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    auto self = reinterpret_cast<TabStrip*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<TabStrip*>(reinterpret_cast<CREATESTRUCTW*>(lparam)->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    // Child messages (including direct DPI/theme changes) need their own keepalive,
    // not just the root callback's guard. A weak reference avoids an ownership cycle.
    const auto lifetime = self ? self->lifetime_.lock() : std::shared_ptr<void>{};
    if (!lifetime) return DefWindowProcW(window, message, wparam, lparam);
    const auto result = self->Handle(message, wparam, lparam);
    if (message == WM_NCDESTROY) { self->window_ = nullptr; SetWindowLongPtrW(window, GWLP_USERDATA, 0); }
    return result;
}

LRESULT TabStrip::Handle(UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_MOUSEACTIVATE: if (classic_caption_) return MA_NOACTIVATE; break;
    case WM_PAINT: Paint(); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_MOUSEMOVE: {
        hover_x_ = GET_X_LPARAM(lparam); hover_y_ = GET_Y_LPARAM(lparam);
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window_, 0};
        TrackMouseEvent(&tracking); InvalidateRect(window_, nullptr, FALSE); return 0;
    }
    case WM_MOUSELEAVE:
        hover_x_ = hover_y_ = -1; InvalidateRect(window_, nullptr, FALSE); return 0;
    case WM_LBUTTONUP: {
        const auto hit = HitTest(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        if (hit.tab || hit.action == TabInput::New) input_(hit.action, hit.tab);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (classic_caption_) SetForegroundWindow(parent_);
        const auto hit = HitTest(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
        if (!hit.tab && hit.action != TabInput::New) {
            POINT point{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ClientToScreen(window_, &point);
            SendMessageW(parent_, WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK:
        if (!HitTest(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)).tab) {
            SendMessageW(parent_, WM_SYSCOMMAND, IsZoomed(parent_) ? SC_RESTORE : SC_MAXIMIZE, 0);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (GET_WHEEL_DELTA_WPARAM(wparam) > 0) { if (first_) --first_; }
        else if (first_ + shown_ < state_.count) ++first_;
        InvalidateRect(window_, nullptr, FALSE); return 0;
    case WM_THEMECHANGED: case WM_SETTINGCHANGE: case WM_DPICHANGED:
        Resize(); return 0;
    }
    return DefWindowProcW(window_, message, wparam, lparam);
}

} // namespace quickky

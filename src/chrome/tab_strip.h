#pragma once

#include "common/hook_protocol.h"

#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>

#include <functional>
#include <memory>

namespace quickky {

// Explorer-UI-thread presenter; the classic caption surface is an owned,
// non-activating tool window, while the diagnostic surface is a child.
// It sends intents without owning group policy or changing Explorer content.
// Its weak attachment reference is locked by each surface callback; root callers
// also retain that owner across Create/Configure/Resize and reentrant teardown.
class TabStrip {
public:
    TabStrip() = default;
    TabStrip(const TabStrip&) = delete;
    TabStrip& operator=(const TabStrip&) = delete;
    using Input = std::function<void(TabInput, std::uint64_t)>;
    ~TabStrip();
    bool Create(HWND parent, Input input, std::weak_ptr<void> lifetime, bool classic_caption = false);
    bool Configure(const GuardRequest& request);
    bool Resize();
    void Hide(bool release_resources = false);
    void RefreshTheme();
    bool alive() const noexcept { return window_ != nullptr; }

private:
    friend struct TabStripTestAccess;
    struct Hit { TabInput action; std::uint64_t tab; };
    static LRESULT CALLBACK Procedure(HWND, UINT, WPARAM, LPARAM);
    LRESULT Handle(UINT, WPARAM, LPARAM);
    bool Resources();
    void Paint();
    void Layout();
    bool UpdateHover(int x, int y);
    Hit HitTest(int x, int y) const;

    HWND window_ = nullptr;
    HWND parent_ = nullptr;
    Input input_;
    std::weak_ptr<void> lifetime_;
    GuardRequest state_;
    Microsoft::WRL::ComPtr<ID2D1Factory> factory_;
    Microsoft::WRL::ComPtr<IDWriteFactory> text_factory_;
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format_;
    Microsoft::WRL::ComPtr<ID2D1HwndRenderTarget> target_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush_;
    float scale_ = 1;
    float corner_radius_ = 0;
    float tab_width_ = 180;
    unsigned first_ = 0;
    unsigned shown_ = 0;
    int height_ = 40;
    int hover_x_ = -1;
    int hover_y_ = -1;
    bool classic_caption_ = false;
    bool resizing_ = false;
    bool tracking_mouse_ = false;
    bool light_ = true;
    bool high_contrast_ = false;
    D2D1_COLOR_F background_{};
    D2D1_COLOR_F foreground_{};
};

} // namespace quickky

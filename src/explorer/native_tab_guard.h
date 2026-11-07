#pragma once

#include <windows.h>
#include <cstdint>
#include <functional>
#include <memory>

namespace quickky {

// Target-UI-thread creation veto plus optional reversible native-band clipping.
// Creation count alone is not a layout signature or proof of every command path.
class NativeTabGuard {
public:
    NativeTabGuard() = default;
    NativeTabGuard(const NativeTabGuard&) = delete;
    NativeTabGuard& operator=(const NativeTabGuard&) = delete;
    ~NativeTabGuard() { Stop(); }
    bool Start(HWND target);
    // Workspace only: remove the native tab band before creating custom chrome.
    // The guard-only probe deliberately does not call this.
    bool SuppressBar(std::weak_ptr<void> owner, std::function<void()> lost);
    void Stop();
    static unsigned CountTabs(HWND target);
    unsigned blocked() const noexcept { return blocked_; }

private:
    static LRESULT CALLBACK CreateHook(int code, WPARAM wparam, LPARAM lparam);
    static LRESULT CALLBACK BarProcedure(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);
    bool ApplyBarClip();
    void RestoreBar();
    static thread_local NativeTabGuard* current_;
    HWND target_ = nullptr;
    HHOOK hook_ = nullptr;
    unsigned blocked_ = 0;
    HWND bar_ = nullptr;
    HRGN original_region_ = nullptr;
    bool updating_ = false;
    bool restore_pending_ = false;
    std::weak_ptr<void> owner_;
    std::function<void()> lost_;
};

} // namespace quickky

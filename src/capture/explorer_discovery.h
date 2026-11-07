#pragma once

#include "common/window_identity.h"

#include <windows.h>

#include <functional>
#include <map>
#include <optional>

namespace quickky {

enum class WindowChangeKind { Added, Removed, Foreground };

struct WindowChange {
    WindowChangeKind kind;
    WindowCandidate window;
};

class ExplorerDiscovery {
public:
    using Inspector = std::function<std::optional<WindowCandidate>(HWND)>;
    using Handler = std::function<void(const WindowChange&)>;
    using RetryHandler = std::function<void()>;

    ExplorerDiscovery(Inspector inspector, Handler handler, RetryHandler retry = {});
    ~ExplorerDiscovery();
    ExplorerDiscovery(const ExplorerDiscovery&) = delete;
    ExplorerDiscovery& operator=(const ExplorerDiscovery&) = delete;

    bool Start();
    void Stop() noexcept;
    bool Refresh() noexcept;
    bool failed() const noexcept { return failed_; }
    const std::map<NativeWindow, WindowCandidate>& windows() const noexcept { return windows_; }

private:
    struct Enumeration;
    static BOOL CALLBACK Enumerate(HWND window, LPARAM parameter) noexcept;
    static void CALLBACK WinEvent(HWINEVENTHOOK, DWORD event, HWND window,
        LONG object, LONG child, DWORD, DWORD) noexcept;
    void OnEvent(DWORD event, HWND window, LONG object, LONG child);
    void RequestRetry(HWND window);

    static thread_local ExplorerDiscovery* current_;
    Inspector inspector_;
    Handler handler_;
    RetryHandler retry_;
    std::map<NativeWindow, WindowCandidate> windows_;
    HWINEVENTHOOK lifecycle_ = nullptr;
    HWINEVENTHOOK foreground_ = nullptr;
    bool refreshing_ = false;
    bool refresh_pending_ = false;
    bool failed_ = false;
};

} // namespace quickky

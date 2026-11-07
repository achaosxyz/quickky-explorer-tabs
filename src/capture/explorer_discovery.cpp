#include "capture/explorer_discovery.h"

#include <array>
#include <cwchar>
#include <utility>
#include <vector>

namespace quickky {

thread_local ExplorerDiscovery* ExplorerDiscovery::current_ = nullptr;

struct ExplorerDiscovery::Enumeration {
    ExplorerDiscovery* discovery;
    std::map<NativeWindow, WindowCandidate> windows;
    bool failed = false;
};

ExplorerDiscovery::ExplorerDiscovery(Inspector inspector, Handler handler, RetryHandler retry)
    : inspector_(std::move(inspector)), handler_(std::move(handler)), retry_(std::move(retry))
{
}

ExplorerDiscovery::~ExplorerDiscovery()
{
    Stop();
}

bool ExplorerDiscovery::Start()
{
    if (current_ != nullptr || !inspector_ || !handler_) {
        return false;
    }
    failed_ = false;
    current_ = this;
    lifecycle_ = SetWinEventHook(EVENT_OBJECT_CREATE, EVENT_OBJECT_HIDE, nullptr,
        WinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    foreground_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, WinEvent, 0, 0, WINEVENT_OUTOFCONTEXT);
    if (lifecycle_ == nullptr || foreground_ == nullptr || !Refresh()) {
        Stop();
        return false;
    }
    return true;
}

void ExplorerDiscovery::Stop() noexcept
{
    if (reconcile_timer_) KillTimer(nullptr, reconcile_timer_);
    reconcile_timer_ = 0;
    pending_foreground_ = nullptr;
    if (lifecycle_ != nullptr) {
        UnhookWinEvent(lifecycle_);
        lifecycle_ = nullptr;
    }
    if (foreground_ != nullptr) {
        UnhookWinEvent(foreground_);
        foreground_ = nullptr;
    }
    if (current_ == this) {
        current_ = nullptr;
    }
    windows_.clear();
}

BOOL CALLBACK ExplorerDiscovery::Enumerate(HWND window, LPARAM parameter) noexcept
{
    auto& enumeration = *reinterpret_cast<Enumeration*>(parameter);
    try {
        if (const auto candidate = enumeration.discovery->inspector_(window)) {
            if (!candidate->valid() || candidate->handle != reinterpret_cast<NativeWindow>(window)) {
                enumeration.failed = true;
                return FALSE;
            }
            enumeration.windows.emplace(candidate->handle, *candidate);
        }
        return TRUE;
    } catch (...) {
        enumeration.failed = true;
        return FALSE;
    }
}

bool ExplorerDiscovery::Refresh() noexcept
{
    if (failed_ || current_ != this) {
        return false;
    }
    if (refreshing_) {
        refresh_pending_ = true;
        return true;
    }
    if (reconcile_timer_) KillTimer(nullptr, reconcile_timer_);
    reconcile_timer_ = 0;
    refreshing_ = true;
    try {
        do {
            refresh_pending_ = false;
            Enumeration enumeration{this, {}};
            if (!EnumWindows(Enumerate, reinterpret_cast<LPARAM>(&enumeration)) || enumeration.failed) {
                failed_ = true;
                break;
            }

            std::vector<WindowChange> changes;
            for (const auto& previous : windows_) {
                const auto next = enumeration.windows.find(previous.first);
                if (next == enumeration.windows.end() || !(next->second == previous.second)) {
                    changes.push_back({WindowChangeKind::Removed, previous.second});
                }
            }
            for (const auto& next : enumeration.windows) {
                const auto previous = windows_.find(next.first);
                if (previous == windows_.end() || !(previous->second == next.second)) {
                    changes.push_back({WindowChangeKind::Added, next.second});
                }
            }
            windows_ = std::move(enumeration.windows);
            for (const auto& change : changes) {
                handler_(change);
            }
        } while (refresh_pending_ && current_ == this);
        const auto foreground = pending_foreground_;
        pending_foreground_ = nullptr;
        if (!failed_ && foreground && current_ == this) {
            const auto found = windows_.find(reinterpret_cast<NativeWindow>(foreground));
            if (found != windows_.end()) handler_({WindowChangeKind::Foreground, found->second});
            else RequestRetry(foreground);
        }
    } catch (...) {
        failed_ = true;
    }
    refreshing_ = false;
    return !failed_;
}

void CALLBACK ExplorerDiscovery::WinEvent(HWINEVENTHOOK, DWORD event, HWND window,
    LONG object, LONG child, DWORD, DWORD) noexcept
{
    if (current_ == nullptr || current_->failed_) {
        return;
    }
    try {
        current_->OnEvent(event, window, object, child);
    } catch (...) {
        if (current_ != nullptr) {
            current_->failed_ = true;
        }
    }
}

void ExplorerDiscovery::OnEvent(DWORD event, HWND window, LONG object, LONG child)
{
    if (window == nullptr) {
        return;
    }
    if (event != EVENT_SYSTEM_FOREGROUND && (object != OBJID_WINDOW || child != CHILDID_SELF)) return;
    const auto handle = reinterpret_cast<NativeWindow>(window);
    const auto known = windows_.find(handle);
    std::array<wchar_t, 64> class_name{};
    const bool relevant = known != windows_.end() ||
        (GetAncestor(window, GA_ROOT) == window &&
         GetClassNameW(window, class_name.data(), static_cast<int>(class_name.size())) != 0 &&
         std::wcscmp(class_name.data(), L"CabinetWClass") == 0);
    if (event == EVENT_SYSTEM_FOREGROUND) {
        if (relevant) {
            pending_foreground_ = window;
            ScheduleRefresh();
            RequestRetry(window);
        } else pending_foreground_ = nullptr;
        return;
    }
    if (event == EVENT_OBJECT_DESTROY) {
        if (known != windows_.end()) {
            const WindowChange change{WindowChangeKind::Removed, known->second};
            windows_.erase(known);
            handler_(change);
            ScheduleRefresh();
        }
        return;
    }

    if (relevant) {
        ScheduleRefresh();
        if (event == EVENT_OBJECT_CREATE || event == EVENT_OBJECT_SHOW) RequestRetry(window);
    }
}

void ExplorerDiscovery::ScheduleRefresh() noexcept
{
    if (current_ != this || failed_) return;
    if (!reconcile_timer_) {
        reconcile_timer_ = SetTimer(nullptr, 0, 25, Reconcile);
        if (!reconcile_timer_) failed_ = true;
    }
}

void CALLBACK ExplorerDiscovery::Reconcile(HWND, UINT, UINT_PTR timer, DWORD) noexcept
{
    if (current_ && current_->reconcile_timer_ == timer) {
        KillTimer(nullptr, timer);
        current_->reconcile_timer_ = 0;
        current_->Refresh();
    }
}

void ExplorerDiscovery::RequestRetry(HWND window)
{
    // A raw lifecycle event can precede probe readiness. This is only a hint to
    // re-run discovery later, never an admitted identity or attachment request.
    if (!retry_ || failed_ || current_ != this ||
        windows_.count(reinterpret_cast<NativeWindow>(window)) ||
        GetAncestor(window, GA_ROOT) != window || GetWindow(window, GW_OWNER)) return;
    std::array<wchar_t, 64> class_name{};
    if (GetClassNameW(window, class_name.data(), static_cast<int>(class_name.size())) &&
        std::wcscmp(class_name.data(), L"CabinetWClass") == 0) retry_();
}

} // namespace quickky

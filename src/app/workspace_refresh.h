#pragma once

#include <cstdint>

namespace quickky {

// Controller-thread timing policy, independent of Win32 and Explorer mutation.
// Nonzero delays reserve one timer; Consume clears that reservation before work
// so reentrant discovery can request a fresh pass without losing its notification.
class WorkspaceRefreshSchedule {
public:
    unsigned Request(std::uint64_t now) noexcept
    {
        retry_until_ = now + 1000;
        return Schedule(25);
    }

    bool Consume() noexcept
    {
        const bool pending = pending_;
        pending_ = false;
        return pending;
    }

    // Continue for a completed native move or an acknowledged pending restore,
    // never for a timed-out/unconfirmed mutation.
    unsigned AfterRefresh(std::uint64_t now, bool continue_native) noexcept
    {
        return continue_native || now < retry_until_ ? Schedule(100) : 0;
    }

    void Reset() noexcept { pending_ = false; retry_until_ = 0; }

private:
    unsigned Schedule(unsigned delay) noexcept
    {
        if (pending_) return 0;
        pending_ = true;
        return delay;
    }

    std::uint64_t retry_until_ = 0;
    bool pending_ = false;
};

} // namespace quickky

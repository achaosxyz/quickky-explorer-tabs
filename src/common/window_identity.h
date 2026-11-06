#pragma once

#include <cstdint>

namespace quickky {

using NativeWindow = std::uintptr_t;
using TabId = std::uint64_t;

struct WindowCandidate {
    NativeWindow handle = 0;
    std::uint32_t process_id = 0;
    std::uint32_t thread_id = 0;
    std::uint64_t process_started_at = 0;

    bool valid() const noexcept
    {
        return handle != 0 && process_id != 0 && thread_id != 0
            && process_started_at != 0;
    }
};

inline bool operator==(const WindowCandidate& left, const WindowCandidate& right) noexcept
{
    return left.handle == right.handle && left.process_id == right.process_id
        && left.thread_id == right.thread_id
        && left.process_started_at == right.process_started_at;
}

struct WindowIdentity {
    WindowCandidate window;
    std::uint64_t generation = 0;

    bool valid() const noexcept { return window.valid() && generation != 0; }
};

inline bool operator==(const WindowIdentity& left, const WindowIdentity& right) noexcept
{
    return left.window == right.window && left.generation == right.generation;
}

inline bool operator!=(const WindowIdentity& left, const WindowIdentity& right) noexcept
{
    return !(left == right);
}

} // namespace quickky

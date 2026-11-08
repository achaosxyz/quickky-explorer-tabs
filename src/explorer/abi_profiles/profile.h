#pragma once

#include <windows.h>
#include <cstdint>
#include <type_traits>

namespace quickky {

// CodeView identity selects reviewed locators, never a nearest-version guess.
struct NativeImageIdentity {
    GUID pdb{};
    std::uint32_t age = 0;
    std::uint32_t size = 0;
};

// Pointer-free locator data. The target must revalidate the accessor contract
// against its own image before using these RVAs or reading any private object.
struct NativeAbiDescriptor {
    NativeImageIdentity image;
    std::uint32_t window_table = 0;
    std::uint32_t actions_table = 0;
    std::uint32_t lookup = 0;
    std::uint32_t methods[13]{};
    std::uint32_t accessor_rule = 0;
};

// Each versioned file supplies data only. Several maps can share one reviewed
// accessor rule; its implementation and field derivation stay in native_abi.cpp.
struct NativeAbiProfile {
    const char* version = nullptr;
    NativeAbiDescriptor descriptor;
};

static_assert(std::is_trivially_copyable_v<NativeAbiDescriptor>, "ABI locators must be value-only.");

} // namespace quickky

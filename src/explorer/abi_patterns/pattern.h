#pragma once

#include "explorer/native_abi.h"
#include <cstddef>

namespace quickky {

// A reviewed implementation variant, never a DLL identity or an address map.
// Slot 8 is checked by the complete accessor-family recognizer instead of a
// raw hash, so its reviewed references and derived fields may change together.
struct NativeAbiVariant {
    NativeEntryFingerprint entries[13]{};
    NativeEntryFingerprint window_entry{};
};

struct NativeAbiFamily {
    std::uint32_t rule = 0;
    std::uint32_t accessor_length = 0;
    std::uint32_t lookup_operand = 0;
    const NativeAbiVariant* variants = nullptr;
    std::size_t count = 0;
};

const NativeAbiFamily& NativeAbiFamily1();
const NativeAbiFamily& NativeAbiFamily2();
const NativeAbiFamily& NativeAbiFamily3();
const NativeAbiFamily& NativeAbiFamily4();
const NativeAbiFamily& NativeAbiFamily5();
const NativeAbiFamily* BuiltinNativeAbiFamilies(std::size_t& count);

} // namespace quickky

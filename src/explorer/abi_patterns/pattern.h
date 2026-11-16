#pragma once

#include "explorer/native_abi.h"
#include <cstddef>

namespace quickky {

// Only offline-disassembled external address operands may vary. The instruction
// end determines rel32/RIP-relative resolution; identity preserves shared targets
// across the interface, and permissions describe the referenced image section.
struct NativeCodeReference {
    std::uint16_t operand = 0;
    std::uint16_t end = 0;
    std::uint32_t permissions = 0;
    std::uint16_t identity = 0;
};

struct NativeCodePattern {
    const unsigned char* bytes = nullptr;
    std::uint32_t length = 0;
    bool unwind = false;
    const NativeCodeReference* references = nullptr;
    std::size_t count = 0;
};

// Complete byte templates, never a DLL identity or fixed locator. Slot 8 retains
// its family-specific accessor/lookup and storage-layout validation.
struct NativeAbiVariant {
    NativeCodePattern entries[13]{};
    NativeCodePattern window_entry{};
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

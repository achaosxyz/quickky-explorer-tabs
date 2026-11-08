#pragma once

#include "explorer/abi_profiles/profile.h"
#include <cstddef>

namespace quickky {

struct NativeAbiLayout {
    // actions is relative to the window object; tabs/active are relative to its
    // tab-actions interface. These offsets are derived, never guessed from a map.
    std::uint32_t actions = 0;
    std::uint32_t tabs = 0;
    std::uint32_t active = 0;
};

// Read-only PE/contract checks used by target binding and isolated byte tests.
// No symbol loading, network, native calls, or object-layout guessing occurs here.
// The range guard grants no lifetime ownership: callers stay on the owning UI
// thread and retain their native object while reading its storage.
bool IsNativeMemoryReadable(const void* pointer, std::size_t size);
bool InspectNativeImage(const unsigned char* image, std::size_t size, NativeImageIdentity& identity);
bool ValidateNativeAbi(const unsigned char* image, std::size_t size, std::uintptr_t base,
    const NativeAbiDescriptor& descriptor, NativeAbiLayout& layout);
const NativeAbiProfile* BuiltinNativeAbiProfiles(std::size_t& count);
// Reset outputs on failure. Identity matches must also pass their accessor rule;
// a rejected entry does not prevent a later independently reviewed map from binding.
bool SelectNativeAbi(const unsigned char* image, std::size_t size, std::uintptr_t base,
    const NativeAbiProfile* profiles, std::size_t count,
    NativeAbiDescriptor& descriptor, NativeAbiLayout& layout);

} // namespace quickky

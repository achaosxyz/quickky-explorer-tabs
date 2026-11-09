#pragma once

#include <windows.h>
#include <cstdint>
#include <type_traits>
#include <cstddef>

namespace quickky {

// Identity of the inspected image. Production discovers locators by code mode;
// versioned reference fixtures use this only as an independent test oracle.
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

// Complete typed-entry contracts, not accessor-only prefix matches. Leaf thunks
// include their full jump/return sequence. Production families reuse this data
// shape without the reference profiles' identities, versions or fixed locators.
struct NativeEntryFingerprint {
    std::uint32_t length = 0;
    bool unwind = false;
    unsigned char sha256[32]{};
};

static_assert(std::is_trivially_copyable_v<NativeAbiDescriptor>, "ABI locators must be value-only.");

struct NativeAbiLayout {
    // actions is relative to the window object; tabs/active are relative to its
    // tab-actions interface. These offsets are derived, never guessed from a map.
    std::uint32_t actions = 0;
    std::uint32_t tabs = 0;
    std::uint32_t active = 0;
};

// Read-only file/module diagnostic, not a claim about a live Explorer transfer.
// Windows may retain an already-loaded module; its checked version is reported.
struct NativeAbiFileStatus {
    std::uint64_t file_version = 0;
    bool inspected = false;
    bool supported = false;
};
NativeAbiFileStatus InspectNativeAbiFile(const wchar_t* path);

// Read-only PE/contract checks used by target binding and isolated byte tests.
// No symbol loading, network, private Explorer calls or layout guessing occurs here.
// The range guard grants no lifetime ownership: callers stay on the owning UI
// thread and retain their native object while reading its storage.
bool IsNativeMemoryReadable(const void* pointer, std::size_t size);
std::uint64_t NativeFileVersionResource(const void* bytes, std::size_t size);
std::uint64_t NativeModuleVersion(HMODULE module);
bool InspectNativeImage(const unsigned char* image, std::size_t size, NativeImageIdentity& identity);
bool ValidateNativeAbi(const unsigned char* image, std::size_t size, std::uintptr_t base,
    const NativeAbiDescriptor& descriptor, NativeAbiLayout& layout);
// Production recognition: discover actual tables and entries by code contract.
// No fixture, component version, fixed RVA or CodeView allowlist is consulted.
// Ambiguity clears both outputs; recognition performs no private native calls.
bool RecognizeNativeAbi(const unsigned char* image, std::size_t size, std::uintptr_t base,
    NativeAbiDescriptor& descriptor, NativeAbiLayout& layout);
} // namespace quickky

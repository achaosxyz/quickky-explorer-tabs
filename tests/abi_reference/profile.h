#pragma once

#include "explorer/native_abi.h"

namespace quickky {

constexpr std::uint64_t PackNativeFileVersion(std::uint16_t major, std::uint16_t minor,
    std::uint16_t build, std::uint16_t revision)
{
    return (static_cast<std::uint64_t>(major) << 48) |
        (static_cast<std::uint64_t>(minor) << 32) |
        (static_cast<std::uint64_t>(build) << 16) | revision;
}

// Exact-image oracle data, linked only into offline ABI tests. Production code
// does not select these records or consult their numeric versions and RVAs.
struct NativeAbiProfile {
    const char* version = nullptr;
    NativeAbiDescriptor descriptor;
    NativeEntryFingerprint entry_fingerprints[13]{};
    std::uint64_t file_version = 0;
    std::uint32_t data_diagnostics[4]{};
};

const NativeAbiProfile& ExplorerFrameAbi26100_1();
const NativeAbiProfile& ExplorerFrameAbi22621_5413();
const NativeAbiProfile& ExplorerFrameAbi22621_5331();
const NativeAbiProfile& ExplorerFrameAbi22621_5262();
const NativeAbiProfile& ExplorerFrameAbi22621_3958();
const NativeAbiProfile& ExplorerFrameAbi22621_4541();
const NativeAbiProfile& ExplorerFrameAbi22621_4601();
const NativeAbiProfile& ExplorerFrameAbi22621_4746();
const NativeAbiProfile& ExplorerFrameAbi26100_7309();
const NativeAbiProfile& ExplorerFrameAbi26100_7462();
const NativeAbiProfile& ExplorerFrameAbi26100_7705();
const NativeAbiProfile& ExplorerFrameAbi26100_7824();
const NativeAbiProfile& ExplorerFrameAbi26100_7920();
const NativeAbiProfile& ExplorerFrameAbi26100_8115();
const NativeAbiProfile& ExplorerFrameAbi26100_8328();
const NativeAbiProfile& ExplorerFrameAbi26100_8457();
const NativeAbiProfile& ExplorerFrameAbi26100_8521();
const NativeAbiProfile& ExplorerFrameAbi22621_3672();
const NativeAbiProfile& ExplorerFrameAbi26100_5074();
const NativeAbiProfile& ExplorerFrameAbi26100_6725();
const NativeAbiProfile& ExplorerFrameAbi26100_7019();
const NativeAbiProfile& ExplorerFrameAbi26100_7171();
const NativeAbiProfile& ExplorerFrameAbi22621_5909();
const NativeAbiProfile& ExplorerFrameAbi22621_6197();
const NativeAbiProfile& ExplorerFrameAbi26100_9818();
const NativeAbiProfile& ExplorerFrameAbi26100_9549();
const NativeAbiProfile& ExplorerFrameAbi26100_9278();
const NativeAbiProfile& ExplorerFrameAbi26100_8972();
const NativeAbiProfile& ExplorerFrameAbi26100_8737();
const NativeAbiProfile& ExplorerFrameAbi26100_8655();
const NativeAbiProfile& ExplorerFrameAbi28000_3086();
const NativeAbiProfile& ExplorerFrameAbi28000_2605();
const NativeAbiProfile& ExplorerFrameAbi28000_2525();
const NativeAbiProfile& ExplorerFrameAbi28000_1761();
const NativeAbiProfile& ExplorerFrameAbi26100_4202();
const NativeAbiProfile& ExplorerFrameAbi26100_4768();
const NativeAbiProfile& ExplorerFrameAbi26100_4484();
const NativeAbiProfile& ExplorerFrameAbi26100_2454();
const NativeAbiProfile& ExplorerFrameAbi26100_2605();
const NativeAbiProfile& ExplorerFrameAbi26100_2894();
const NativeAbiProfile& ExplorerFrameAbi26100_3037();
const NativeAbiProfile& ExplorerFrameAbi26100_3323();
const NativeAbiProfile& ExplorerFrameAbi26100_3624();
const NativeAbiProfile& ExplorerFrameAbi26100_3912();
const NativeAbiProfile& ExplorerFrameAbi26100_4061();
const NativeAbiProfile& ExplorerFrameAbi26100_1150();
const NativeAbiProfile& ExplorerFrameAbi26100_1455();
const NativeAbiProfile& ExplorerFrameAbi26100_1591();
const NativeAbiProfile& ExplorerFrameAbi26100_1882();
const NativeAbiProfile& ExplorerFrameAbi26100_2161();
const NativeAbiProfile& ExplorerFrameAbi26100_2314();
const NativeAbiProfile& ExplorerFrameAbi26100_1000();
const NativeAbiProfile& ExplorerFrameAbi26100_712();
const NativeAbiProfile& ExplorerFrameAbi22621_5983();

// Legacy exact-map and fallback regression oracles, defined in the test fixture
// library only. Neither function is linked into the application/hook product.
const NativeAbiProfile* BuiltinNativeAbiProfiles(std::size_t& count);
bool SelectNativeAbi(const unsigned char* image, std::size_t size, std::uintptr_t base,
    const NativeAbiProfile* profiles, std::size_t count,
    NativeAbiDescriptor& descriptor, NativeAbiLayout& layout,
    std::uint64_t file_version = 0, const NativeAbiProfile** matched = nullptr);

} // namespace quickky

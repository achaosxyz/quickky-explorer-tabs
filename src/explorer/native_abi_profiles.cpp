#include "explorer/native_abi.h"
#include "explorer/abi_profiles/10.0.26100.9949.h"

namespace quickky {

const NativeAbiProfile* BuiltinNativeAbiProfiles(std::size_t& count)
{
    // Register separately reviewed version files here; never synthesize maps by
    // shifting RVAs or copying a nearby version's identity.
    static constexpr NativeAbiProfile profiles[]{kExplorerFrame_10_0_26100_9949};
    count = sizeof(profiles) / sizeof(profiles[0]);
    return profiles;
}

} // namespace quickky

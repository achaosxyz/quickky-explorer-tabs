#include "explorer/native_abi.h"

namespace quickky {

const NativeAbiProfile* BuiltinNativeAbiProfiles(std::size_t& count)
{
    // Register separately reviewed version files here; never synthesize maps by
    // shifting RVAs or copying a nearby version's identity.
    static const NativeAbiProfile profiles[]{
        ExplorerFrameAbi26100_9549(), ExplorerFrameAbi26100_9818(), ExplorerFrameAbi26100_1(),
        ExplorerFrameAbi28000_3086(), ExplorerFrameAbi28000_2605(), ExplorerFrameAbi28000_2525(),
        ExplorerFrameAbi28000_1761(), ExplorerFrameAbi26100_9278(), ExplorerFrameAbi26100_8972(),
        ExplorerFrameAbi26100_8737(), ExplorerFrameAbi26100_8655(), ExplorerFrameAbi22621_5413(),
        ExplorerFrameAbi22621_5331(), ExplorerFrameAbi22621_5262(), ExplorerFrameAbi22621_3958(),
        ExplorerFrameAbi22621_4541(), ExplorerFrameAbi22621_4601(), ExplorerFrameAbi22621_4746(),
        ExplorerFrameAbi26100_7309(), ExplorerFrameAbi26100_7462(), ExplorerFrameAbi26100_7705(),
        ExplorerFrameAbi26100_7824(), ExplorerFrameAbi26100_7920(), ExplorerFrameAbi26100_8115(),
        ExplorerFrameAbi26100_8328(), ExplorerFrameAbi26100_8457(), ExplorerFrameAbi26100_8521(),
        ExplorerFrameAbi22621_3672(), ExplorerFrameAbi26100_5074(), ExplorerFrameAbi26100_6725(),
        ExplorerFrameAbi26100_7019(), ExplorerFrameAbi26100_7171(), ExplorerFrameAbi22621_5909(),
        ExplorerFrameAbi22621_6197(), ExplorerFrameAbi26100_4202(), ExplorerFrameAbi26100_4768(),
        ExplorerFrameAbi26100_4484(), ExplorerFrameAbi26100_2454(), ExplorerFrameAbi26100_2605(),
        ExplorerFrameAbi26100_2894(), ExplorerFrameAbi26100_3037(), ExplorerFrameAbi26100_3323(),
        ExplorerFrameAbi26100_3624(), ExplorerFrameAbi26100_3912(), ExplorerFrameAbi26100_4061(),
        ExplorerFrameAbi26100_1150(), ExplorerFrameAbi26100_1455(), ExplorerFrameAbi26100_1591(),
        ExplorerFrameAbi26100_1882(), ExplorerFrameAbi26100_2161(), ExplorerFrameAbi26100_2314(),
        ExplorerFrameAbi26100_1000(), ExplorerFrameAbi26100_712(), ExplorerFrameAbi22621_5983()
    };
    count = sizeof(profiles) / sizeof(profiles[0]);
    return profiles;
}

} // namespace quickky

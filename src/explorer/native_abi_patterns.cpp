#include "explorer/abi_patterns/pattern.h"

namespace quickky {

const NativeAbiFamily* BuiltinNativeAbiFamilies(std::size_t& count)
{
    // This registry contains code contracts only. Reference DLL identities and
    // fixed RVAs are linked separately into the offline test fixture library.
    static const NativeAbiFamily families[]{
        NativeAbiFamily1(), NativeAbiFamily2(), NativeAbiFamily3(),
        NativeAbiFamily4(), NativeAbiFamily5()
    };
    count = sizeof(families) / sizeof(families[0]);
    return families;
}

} // namespace quickky

#pragma once

#include "explorer/abi_profiles/profile.h"

namespace quickky {

// Exact public-PDB locators verified against scoped native frames. Version names
// organize evidence; the image identity and accessor rule authorize binding.
inline constexpr NativeAbiProfile kExplorerFrame_10_0_26100_9949{
    "10.0.26100.9949",
    {{{0x822a8c93,0x960a,0xc7b6,{0x7f,0x6a,0x87,0xcf,0x48,0xe8,0xce,0x6f}},1,0x323000},
        0x27b2a0,0x27b090,0x7dde4,
        {0x1c8b10,0x1ba8c0,0x1c97c0,0x1ba950,0x1c98b0,0x12e640,0x7d5a0,
         0x10f870,0x7d890,0x1ba980,0x1c5e40,0x1c9f30,0x1c9f10},
        1}};

} // namespace quickky

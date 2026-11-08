#pragma once

#include "explorer/native_abi.h"

namespace quickky {
namespace detail {

// Version units recognize their accessor shape; the opaque view and common
// validator retain image bounds, hashing and shared storage invariants.
struct NativeImageView;
bool CheckDataAccessor(const NativeImageView& image, const NativeAbiDescriptor& descriptor,
    unsigned length, const unsigned* operands, std::size_t count,
    const unsigned char* digest, unsigned active_operand, NativeAbiLayout& layout,
    unsigned actions_operand = 0x4a, unsigned lookup_operand = 0x60,
    bool split_lookup = false);
bool CheckAccessorRule1(const NativeImageView& image, const NativeAbiDescriptor& descriptor,
    NativeAbiLayout& layout);
bool CheckAccessorRule2(const NativeImageView& image, const NativeAbiDescriptor& descriptor,
    NativeAbiLayout& layout);
bool CheckAccessorRule3(const NativeImageView& image, const NativeAbiDescriptor& descriptor,
    NativeAbiLayout& layout);
bool CheckAccessorRule4(const NativeImageView& image, const NativeAbiDescriptor& descriptor,
    NativeAbiLayout& layout);
bool CheckAccessorRule5(const NativeImageView& image, const NativeAbiDescriptor& descriptor,
    NativeAbiLayout& layout);

} // namespace detail
} // namespace quickky

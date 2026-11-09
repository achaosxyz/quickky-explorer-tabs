#pragma once

#include "explorer/native_abi.h"
#include <algorithm>

namespace quickky {
namespace detail {

// Family units recognize accessor shapes; the shared view and validator retain
// image bounds, hashing and storage invariants. This is not an object scanner.
// Tests expand file images to RVAs;
// production checks the actual loaded image on the owning Explorer thread.
struct NativeImageView {
    const unsigned char* bytes;
    std::size_t size;
    const IMAGE_NT_HEADERS64* nt = nullptr;
    const IMAGE_SECTION_HEADER* sections = nullptr;

    bool Range(std::size_t rva, std::size_t length) const
    { return rva <= size && length <= size - rva; }

    bool Open()
    {
        if (!bytes || !Range(0, sizeof(IMAGE_DOS_HEADER))) return false;
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
            !Range(dos->e_lfanew, sizeof(IMAGE_NT_HEADERS64))) return false;
        nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            nt->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER64) ||
            nt->OptionalHeader.NumberOfRvaAndSizes < IMAGE_NUMBEROF_DIRECTORY_ENTRIES ||
            nt->OptionalHeader.SizeOfImage != size || !nt->FileHeader.NumberOfSections ||
            nt->FileHeader.NumberOfSections > 96) return false;
        const auto offset = static_cast<std::size_t>(dos->e_lfanew) + sizeof(*nt);
        if (!Range(offset, nt->FileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER))) return false;
        sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(bytes + offset);
        return true;
    }

    bool Section(std::uint32_t rva, std::size_t length, bool executable) const
    {
        if (!Range(rva, length)) return false;
        for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
            const auto& s = sections[i];
            const auto extent = std::max(s.Misc.VirtualSize, s.SizeOfRawData);
            if (rva < s.VirtualAddress || rva - s.VirtualAddress > extent ||
                length > extent - (rva - s.VirtualAddress)) continue;
            return (s.Characteristics & IMAGE_SCN_MEM_READ) &&
                !(s.Characteristics & IMAGE_SCN_MEM_WRITE) &&
                !!(s.Characteristics & IMAGE_SCN_MEM_EXECUTE) == executable;
        }
        return false;
    }

    bool Function(std::uint32_t rva, std::size_t length) const
    {
        const auto d = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        if (!d.Size || d.Size % sizeof(RUNTIME_FUNCTION) || !Range(d.VirtualAddress, d.Size) ||
            !Section(rva, length, true)) return false;
        const auto functions = reinterpret_cast<const RUNTIME_FUNCTION*>(bytes + d.VirtualAddress);
        for (unsigned i = 0; i < d.Size / sizeof(*functions); ++i) {
            if (functions[i].BeginAddress == rva &&
                static_cast<std::uint64_t>(rva) + length == functions[i].EndAddress) return true;
        }
        return false;
    }
};
bool CheckAccessor(const NativeImageView& image, const NativeAbiDescriptor& descriptor,
    NativeAbiLayout& layout);
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

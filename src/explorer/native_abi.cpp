#include "explorer/native_abi.h"

#include <bcrypt.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

namespace quickky {
namespace {

// A bounded image view, not an object scanner. Tests expand file images to RVAs;
// production checks the actual loaded image on the owning Explorer thread.
struct Image {
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

template<class T> T Read(const unsigned char* p)
{
    T value{};
    std::memcpy(&value, p, sizeof(value));
    return value;
}

// Reviewed instruction operands in the accessor family from the verified PDB.
// Normalize only external rel32/RIP references, decoded object displacements,
// and four source-line immediates passed to WIL Return_Hr. The surrounding
// mov/call instructions still have to match; no arbitrary constants are masked.
// All other bytes (branches, record stride/fields, ownership/output operations)
// remain part of the fingerprint. A different code shape needs explicit review.
constexpr unsigned kDataOperands[]{0x16,0x4a,0x60,0x7f,0x9b,0xab,0xb7,0xc2,0xe8,
    0xf2,0xfd,0x12d,0x14a,0x177,0x194,0x19f,0x1a9,0x1bb,0x1e8,0x1ed,0x1f2,0x208,
    0x20d,0x212,0x21d,0x233,0x238,0x23d,0x248,0x253,0x25e,0x277,0x27c,0x281,0x28d,0x298,0x2a3,0x2ae,0x2b9};
constexpr unsigned kLookupOperands[]{0x15,0x1c,0x5b,0x73,0x92,0xac};
constexpr unsigned char kDataDigest[]{0x24,0xb3,0x28,0xad,0x8a,0x83,0x26,0x85,
    0xd8,0xed,0x9d,0xe1,0x04,0x82,0xc1,0xc7,0x36,0x5d,0xfe,0xed,0x46,0x84,0x88,0xed,
    0x22,0x64,0xcc,0x5f,0x8a,0x46,0x17,0xd4};
constexpr unsigned char kLookupDigest[]{0x29,0xc3,0x4c,0xf1,0xb7,0x41,0x90,0x12,
    0xe0,0x3e,0xef,0xc6,0xba,0x18,0xab,0x09,0xd2,0x4f,0xbe,0x84,0x47,0x00,0x9c,0xc7,
    0xd3,0xdd,0x05,0x19,0x40,0x48,0x38,0xe5};

template<std::size_t N> bool Fingerprint(const Image& image, std::uint32_t rva,
    unsigned length, const unsigned (&operands)[N], const unsigned char (&expected)[32])
{
    if (!image.Function(rva, length)) return false;
    std::vector<unsigned char> bytes(image.bytes + rva, image.bytes + rva + length);
    for (auto offset : operands) std::fill_n(bytes.begin() + offset, 4, static_cast<unsigned char>(0));
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::array<unsigned char, 32> digest{};
    bool match = false;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0 &&
        BCryptHashData(hash, bytes.data(), length, 0) >= 0 &&
        BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0) {
        match = std::equal(digest.begin(), digest.end(), expected);
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return match;
}
} // namespace

bool IsNativeMemoryReadable(const void* pointer, std::size_t size)
{
    MEMORY_BASIC_INFORMATION region{};
    if (!pointer || !size || !VirtualQuery(pointer, &region, sizeof(region)) ||
        region.State != MEM_COMMIT || (region.Protect & PAGE_GUARD)) return false;
    switch (region.Protect & 0xff) {
    case PAGE_READONLY:
    case PAGE_READWRITE:
    case PAGE_WRITECOPY:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        break;
    default:
        return false;
    }
    const auto p = reinterpret_cast<std::uintptr_t>(pointer);
    const auto first = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    return p >= first && size <= region.RegionSize - (p - first);
}

bool InspectNativeImage(const unsigned char* bytes, std::size_t size, NativeImageIdentity& identity)
{
    identity = {};
    NativeImageIdentity parsed;
    Image image{bytes, size};
    if (!image.Open()) return false;
    const auto d = image.nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (!d.Size || d.Size % sizeof(IMAGE_DEBUG_DIRECTORY) || !image.Range(d.VirtualAddress, d.Size)) return false;
    const auto entries = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(bytes + d.VirtualAddress);
    bool found = false;
    for (unsigned i = 0; i < d.Size / sizeof(*entries); ++i) {
        const auto& entry = entries[i];
        if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW) continue;
        if (found || entry.SizeOfData < 24 || !image.Range(entry.AddressOfRawData, entry.SizeOfData)) return false;
        const auto record = bytes + entry.AddressOfRawData;
        if (std::memcmp(record, "RSDS", 4)) return false;
        parsed.pdb = Read<GUID>(record + 4);
        parsed.age = Read<DWORD>(record + 20);
        parsed.size = static_cast<std::uint32_t>(size);
        if (!parsed.age || InlineIsEqualGUID(parsed.pdb, GUID{})) return false;
        found = true;
    }
    if (found) identity = parsed;
    return found;
}

bool ValidateNativeAbi(const unsigned char* bytes, std::size_t size, std::uintptr_t base,
    const NativeAbiDescriptor& descriptor, NativeAbiLayout& layout)
{
    layout = {};
    if (descriptor.accessor_rule != 1) return false;
    NativeImageIdentity actual;
    Image image{bytes, size};
    if (!image.Open() || !InspectNativeImage(bytes, size, actual) ||
        !InlineIsEqualGUID(actual.pdb, descriptor.image.pdb) || actual.age != descriptor.image.age ||
        actual.size != descriptor.image.size || !image.Section(descriptor.window_table, 8, false) ||
        !image.Section(descriptor.actions_table, sizeof(descriptor.methods) / 4 * 8, false)) return false;
    if (!base) base = static_cast<std::uintptr_t>(image.nt->OptionalHeader.ImageBase);
    const auto window_method = Read<std::uint64_t>(bytes + descriptor.window_table);
    if (window_method < base || window_method - base > MAXDWORD ||
        !image.Section(static_cast<DWORD>(window_method - base), 1, true)) return false;
    for (unsigned i = 0; i < 13; ++i) {
        if (!image.Section(descriptor.methods[i], 1, true) ||
            Read<std::uint64_t>(bytes + descriptor.actions_table + i * 8) !=
                static_cast<std::uint64_t>(base) + descriptor.methods[i]) return false;
    }
    const auto data = descriptor.methods[8];
    if (!Fingerprint(image, data, 0x2cd, kDataOperands, kDataDigest) ||
        !Fingerprint(image, descriptor.lookup, 0xe3, kLookupOperands, kLookupDigest)) return false;
    // The normalized call must still name the validated lookup implementation.
    const auto called = static_cast<std::int64_t>(data) + 0x64 + Read<std::int32_t>(bytes + data + 0x60);
    const auto delta = Read<std::int32_t>(bytes + data + 0x4a);
    const auto first = Read<std::uint32_t>(bytes + descriptor.lookup + 0x15);
    const auto last = Read<std::uint32_t>(bytes + descriptor.lookup + 0x1c);
    const auto active = Read<std::uint32_t>(bytes + data + 0x9b);
    const auto actions = -static_cast<std::int64_t>(delta);
    constexpr unsigned limit = 1024 * 1024;
    if (called != descriptor.lookup || actions < 8 || actions > limit || actions % 8 ||
        first <= actions || first > limit || first % 8 || last != first + 8 ||
        active < first - actions + 16 || active > limit || active % 8) return false;
    layout.actions = static_cast<std::uint32_t>(actions);
    layout.tabs = first - layout.actions;
    layout.active = active;
    return true;
}

bool SelectNativeAbi(const unsigned char* bytes, std::size_t size, std::uintptr_t base,
    const NativeAbiProfile* profiles, std::size_t count,
    NativeAbiDescriptor& descriptor, NativeAbiLayout& layout)
{
    descriptor = {};
    layout = {};
    NativeImageIdentity identity;
    if (!profiles || !InspectNativeImage(bytes, size, identity)) return false;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& candidate = profiles[i].descriptor;
        if (!InlineIsEqualGUID(identity.pdb, candidate.image.pdb) ||
            identity.age != candidate.image.age || identity.size != candidate.image.size) continue;
        if (!ValidateNativeAbi(bytes, size, base, candidate, layout)) continue;
        descriptor = candidate;
        return true;
    }
    return false;
}

} // namespace quickky

#include "explorer/native_abi_internal.h"

#include <bcrypt.h>
#include <winver.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <type_traits>
#include <vector>

namespace quickky {
namespace detail {

// A bounded image view, not an object scanner. Tests expand file images to RVAs;
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
} // namespace detail
namespace {
using detail::NativeImageView;

template<class T> T Read(const unsigned char* p)
{
    T value{};
    std::memcpy(&value, p, sizeof(value));
    return value;
}

// The lookup contract is shared by the reviewed accessor rules.
constexpr unsigned kLookupOperands[]{0x15,0x1c,0x5b,0x73,0x92,0xac};
constexpr unsigned char kLookupDigest[]{0x29,0xc3,0x4c,0xf1,0xb7,0x41,0x90,0x12,
    0xe0,0x3e,0xef,0xc6,0xba,0x18,0xab,0x09,0xd2,0x4f,0xbe,0x84,0x47,0x00,0x9c,0xc7,
    0xd3,0xdd,0x05,0x19,0x40,0x48,0x38,0xe5};

// 22621 splits the 40-byte record increment into a cold unwind range. Its
// normalized branch is safe only after validating that complete range/backedge.
constexpr unsigned kSplitLookupOperands[]{0x17,0x21,0x39,0x41,0x58,0x6d,0x86,0x9d};
constexpr unsigned char kSplitLookupDigest[]{0xb7,0x4c,0x2e,0x3b,0x0b,0xda,0xd7,0x12,
    0xc9,0xca,0x81,0x3f,0xe7,0x59,0x4e,0x9e,0x92,0xef,0x9f,0x2a,0xaf,0x02,0xc8,0xa1,
    0x80,0xaf,0xc3,0x64,0xbb,0xc8,0x9b,0x1b};
constexpr unsigned kColdJump[]{5};
constexpr unsigned char kColdDigest[]{0x3d,0xf3,0xc7,0x9c,0x02,0x28,0xf9,0xb5,
    0x14,0xe8,0x72,0x45,0x70,0xcd,0xb9,0xfa,0x51,0xf2,0xbb,0x83,0x20,0xad,0x9f,0x9e,
    0x06,0x3b,0x65,0x5f,0x0a,0xac,0x44,0xf2};

bool DigestMatches(std::vector<unsigned char>& bytes, const unsigned char* expected)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    std::array<unsigned char, 32> digest{};
    bool match = false;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0 &&
        BCryptHashData(hash, bytes.data(), static_cast<ULONG>(bytes.size()), 0) >= 0 &&
        BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0) {
        match = std::equal(digest.begin(), digest.end(), expected);
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return match;
}

bool Fingerprint(const NativeImageView& image, std::uint32_t rva, unsigned length,
    const unsigned* operands, std::size_t count, const unsigned char* expected)
{
    if (!image.Function(rva, length)) return false;
    std::vector<unsigned char> bytes(image.bytes + rva, image.bytes + rva + length);
    for (std::size_t i = 0; i < count; ++i) {
        const auto offset = operands[i];
        if (offset > length || length - offset < 4) return false;
        std::fill_n(bytes.begin() + offset, 4, static_cast<unsigned char>(0));
    }
    return DigestMatches(bytes, expected);
}

bool EntryFingerprintsMatch(const NativeImageView& image, const NativeAbiProfile& profile)
{
    for (unsigned i = 0; i < 13; ++i) {
        const auto rva = profile.descriptor.methods[i];
        const auto& entry = profile.entry_fingerprints[i];
        if (!entry.length || !image.Section(rva, entry.length, true) ||
            (entry.unwind && !image.Function(rva, entry.length))) return false;
        std::vector<unsigned char> code(image.bytes + rva, image.bytes + rva + entry.length);
        if (i == 8) {
            // Fallback is stricter than accessor-family recognition: do not mask
            // relative calls or fields, only the reviewed diagnostic immediates.
            for (const auto offset : profile.data_diagnostics) {
                if (!offset || offset > code.size() || code.size() - offset < 5 ||
                    code[offset - 1] != 0xba || code[offset + 4] != 0xe8) return false;
                std::fill_n(code.begin() + offset, 4, static_cast<unsigned char>(0));
            }
        }
        if (!DigestMatches(code, entry.sha256)) return false;
    }
    return true;
}
} // namespace

std::uint64_t NativeFileVersionResource(const void* data, std::size_t size)
{
    // Parse only VS_VERSION_INFO's fixed binary value, without loading symbols
    // or reading a possibly newer replacement file from disk.
    constexpr wchar_t key[] = L"VS_VERSION_INFO";
    constexpr auto offset = (6 + sizeof(key) + 3) & ~std::size_t(3);
    if (!data || size < offset + sizeof(VS_FIXEDFILEINFO)) return 0;
    const auto bytes = static_cast<const unsigned char*>(data);
    const auto length = Read<WORD>(bytes);
    if (length > size || length < offset + sizeof(VS_FIXEDFILEINFO) ||
        Read<WORD>(bytes + 2) != sizeof(VS_FIXEDFILEINFO) || Read<WORD>(bytes + 4) != 0 ||
        std::memcmp(bytes + 6, key, sizeof(key))) return 0;
    const auto info = Read<VS_FIXEDFILEINFO>(bytes + offset);
    if (info.dwSignature != 0xfeef04bd || info.dwStrucVersion != 0x00010000) return 0;
    return (static_cast<std::uint64_t>(info.dwFileVersionMS) << 32) | info.dwFileVersionLS;
}

std::uint64_t NativeModuleVersion(HMODULE module)
{
    if (!module) return 0;
    const auto resource = FindResourceW(module, MAKEINTRESOURCEW(1), RT_VERSION);
    if (!resource) return 0;
    const auto loaded = LoadResource(module, resource);
    return NativeFileVersionResource(loaded ? LockResource(loaded) : nullptr,
        SizeofResource(module, resource));
}

NativeAbiFileStatus InspectNativeAbiFile(const wchar_t* path)
{
    NativeAbiFileStatus status;
    if (!path || !*path) return status;
    // Request only image layout. Combined datafile flags may return raw layout;
    // Windows may also retain a module the controller already loaded elsewhere.
    const auto module = LoadLibraryExW(path, nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
    if (!module) return status;
    const std::unique_ptr<std::remove_pointer_t<HMODULE>, decltype(&FreeLibrary)> hold(module, FreeLibrary);
    const auto handle = reinterpret_cast<std::uintptr_t>(module);
    const auto tag = handle & 3;
    if (tag != 0 && tag != 2) return status;
    const auto bytes = reinterpret_cast<const unsigned char*>(handle & ~std::uintptr_t{3});
    if (!IsNativeMemoryReadable(bytes, sizeof(IMAGE_DOS_HEADER))) return status;
    const auto dos = Read<IMAGE_DOS_HEADER>(bytes);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(IMAGE_DOS_HEADER)) ||
        !IsNativeMemoryReadable(bytes + dos.e_lfanew, sizeof(IMAGE_NT_HEADERS64))) return status;
    const auto nt = Read<IMAGE_NT_HEADERS64>(bytes + dos.e_lfanew);
    if (nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return status;
    status.file_version = NativeModuleVersion(module);
    status.inspected = status.file_version != 0;
    if (!status.inspected) return status;
    std::size_t count = 0;
    const auto profiles = BuiltinNativeAbiProfiles(count);
    NativeAbiDescriptor descriptor;
    NativeAbiLayout layout;
    // New image resources are unrelocated; existing executable images use ASLR.
    status.supported = SelectNativeAbi(bytes, nt.OptionalHeader.SizeOfImage, tag == 2 ? 0 : handle,
        profiles, count, descriptor, layout, status.file_version);
    return status;
}

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
    NativeImageView image{bytes, size};
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
    NativeImageIdentity actual;
    NativeImageView image{bytes, size};
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
    switch (descriptor.accessor_rule) {
    case 1:
        return detail::CheckAccessorRule1(image, descriptor, layout);
    case 2:
        return detail::CheckAccessorRule2(image, descriptor, layout);
    case 3:
        return detail::CheckAccessorRule3(image, descriptor, layout);
    case 4:
        return detail::CheckAccessorRule4(image, descriptor, layout);
    case 5:
        return detail::CheckAccessorRule5(image, descriptor, layout);
    default:
        return false;
    }
}

bool detail::CheckDataAccessor(const NativeImageView& image, const NativeAbiDescriptor& descriptor,
    unsigned length, const unsigned* operands, std::size_t count,
    const unsigned char* digest, unsigned active_operand, NativeAbiLayout& layout,
    unsigned actions_operand, unsigned lookup_operand, bool split_lookup)
{
    const auto bytes = image.bytes;
    const auto data = descriptor.methods[8];
    if (!Fingerprint(image, data, length, operands, count, digest)) return false;
    if (split_lookup) {
        if (!Fingerprint(image, descriptor.lookup, 0xcf, kSplitLookupOperands,
            sizeof(kSplitLookupOperands) / sizeof(kSplitLookupOperands[0]), kSplitLookupDigest)) return false;
        const auto cold = static_cast<std::int64_t>(descriptor.lookup) + 0x45 +
            Read<std::int32_t>(bytes + descriptor.lookup + 0x41);
        if (cold < 0 || cold > MAXDWORD ||
            !Fingerprint(image, static_cast<DWORD>(cold), 9, kColdJump, 1, kColdDigest) ||
            cold + 9 + Read<std::int32_t>(bytes + cold + 5) !=
                static_cast<std::int64_t>(descriptor.lookup) + 0x28) return false;
    } else if (!Fingerprint(image, descriptor.lookup, 0xe3, kLookupOperands,
        sizeof(kLookupOperands) / sizeof(kLookupOperands[0]), kLookupDigest)) return false;
    // The normalized call must still name the validated lookup implementation.
    const auto called = static_cast<std::int64_t>(data) + lookup_operand + 4 +
        Read<std::int32_t>(bytes + data + lookup_operand);
    const auto delta = Read<std::int32_t>(bytes + data + actions_operand);
    const auto first = Read<std::uint32_t>(bytes + descriptor.lookup + (split_lookup ? 0x17 : 0x15));
    const auto last = Read<std::uint32_t>(bytes + descriptor.lookup + (split_lookup ? 0x21 : 0x1c));
    const auto active = Read<std::uint32_t>(bytes + data + active_operand);
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
    NativeAbiDescriptor& descriptor, NativeAbiLayout& layout,
    std::uint64_t file_version, const NativeAbiProfile** matched)
{
    descriptor = {};
    layout = {};
    if (matched) *matched = nullptr;
    NativeImageIdentity identity;
    if (!profiles || !InspectNativeImage(bytes, size, identity)) return false;
    bool exact_identity = false;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& candidate = profiles[i].descriptor;
        if (!InlineIsEqualGUID(identity.pdb, candidate.image.pdb) ||
            identity.age != candidate.image.age || identity.size != candidate.image.size) continue;
        exact_identity = true;
        if (!ValidateNativeAbi(bytes, size, base, candidate, layout)) continue;
        descriptor = candidate;
        if (matched) *matched = profiles + i;
        return true;
    }
    // A known identity with a failed contract is not an unknown-version upgrade.
    if (exact_identity || !file_version) return false;
    NativeImageView image{bytes, size};
    if (!image.Open()) return false;
    std::vector<const NativeAbiProfile*> candidates;
    for (std::size_t i = 0; i < count; ++i) {
        if (profiles[i].file_version && profiles[i].file_version <= file_version)
            candidates.push_back(profiles + i);
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const auto a, const auto b) {
        return a->file_version > b->file_version;
    });
    for (const auto profile : candidates) {
        if (!EntryFingerprintsMatch(image, *profile)) continue;
        auto candidate = profile->descriptor;
        candidate.image = identity;
        if (!ValidateNativeAbi(bytes, size, base, candidate, layout)) continue;
        descriptor = candidate;
        if (matched) *matched = profile;
        return true;
    }
    return false;
}

} // namespace quickky

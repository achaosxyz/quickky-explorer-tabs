#include "explorer/native_abi_internal.h"
#include "explorer/abi_patterns/pattern.h"

#include <bcrypt.h>
#include <winver.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <map>
#include <type_traits>
#include <vector>

namespace quickky {
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

bool HashBytes(const unsigned char* bytes, std::size_t size, std::array<unsigned char, 32>& digest)
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    bool match = false;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0 &&
        BCryptHashData(hash, const_cast<unsigned char*>(bytes), static_cast<ULONG>(size), 0) >= 0 &&
        BCryptFinishHash(hash, digest.data(), static_cast<ULONG>(digest.size()), 0) >= 0) {
        match = true;
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    return match;
}

bool DigestMatches(const std::vector<unsigned char>& bytes, const unsigned char* expected)
{
    std::array<unsigned char, 32> digest{};
    return HashBytes(bytes.data(), bytes.size(), digest) &&
        std::equal(digest.begin(), digest.end(), expected);
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
    NativeImageIdentity identity;
    status.inspected = InspectNativeImage(bytes, nt.OptionalHeader.SizeOfImage, identity);
    if (!status.inspected) return status;
    NativeAbiDescriptor descriptor;
    NativeAbiLayout layout;
    // New image resources are unrelocated; existing executable images use ASLR.
    status.supported = RecognizeNativeAbi(bytes, nt.OptionalHeader.SizeOfImage, tag == 2 ? 0 : handle,
        descriptor, layout);
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

bool detail::CheckAccessor(const NativeImageView& image, const NativeAbiDescriptor& descriptor,
    NativeAbiLayout& layout)
{
    switch (descriptor.accessor_rule) {
    case 1: return CheckAccessorRule1(image, descriptor, layout);
    case 2: return CheckAccessorRule2(image, descriptor, layout);
    case 3: return CheckAccessorRule3(image, descriptor, layout);
    case 4: return CheckAccessorRule4(image, descriptor, layout);
    case 5: return CheckAccessorRule5(image, descriptor, layout);
    default: return false;
    }
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
    return detail::CheckAccessor(image, descriptor, layout);
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

namespace {

// Per-inspection scratch state. Index image pointers once, and hash complete
// bodies once per (RVA, extent), instead of rescanning for every code variant.
// Nothing here reads Explorer objects or executes any candidate function.
struct PatternScan {
    const NativeImageView& image;
    std::uintptr_t base;
    std::map<DWORD, DWORD> extents;
    std::vector<std::pair<DWORD, DWORD>> pointers;
    std::map<std::uint64_t, std::array<unsigned char, 32>> hashes;

    bool Pointer(std::uint64_t value, std::uint32_t& rva) const
    {
        if (value < base || value - base > MAXDWORD) return false;
        rva = static_cast<DWORD>(value - base);
        return image.Section(rva, 1, true);
    }

    bool Prepare()
    {
        const auto d = image.nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
        // Bound allocation/work before inserting any tree nodes. SizeOfImage
        // alone still permits millions of tiny synthetic RUNTIME_FUNCTIONs.
        constexpr std::size_t max_functions = 65536;
        const auto function_count = d.Size / sizeof(RUNTIME_FUNCTION);
        if (!d.Size || d.Size % sizeof(RUNTIME_FUNCTION) ||
            function_count > max_functions ||
            !image.Section(d.VirtualAddress, d.Size, false)) return false;
        for (std::size_t i = 0; i < function_count; ++i) {
            const auto f = Read<RUNTIME_FUNCTION>(image.bytes + d.VirtualAddress + i * sizeof(RUNTIME_FUNCTION));
            if (f.EndAddress <= f.BeginAddress ||
                !image.Section(f.BeginAddress, f.EndAddress - f.BeginAddress, true) ||
                !extents.emplace(f.BeginAddress, f.EndAddress - f.BeginAddress).second) return false;
        }
        std::size_t scanned = 0;
        for (unsigned i = 0; i < image.nt->FileHeader.NumberOfSections; ++i) {
            const auto& s = image.sections[i];
            if (!(s.Characteristics & IMAGE_SCN_MEM_READ) ||
                (s.Characteristics & (IMAGE_SCN_MEM_WRITE | IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_DISCARDABLE))) continue;
            const auto length = std::max(s.Misc.VirtualSize, s.SizeOfRawData);
            if (!image.Range(s.VirtualAddress, length) || length > 32 * 1024 * 1024 - scanned) return false;
            scanned += length;
            const auto end = static_cast<std::size_t>(s.VirtualAddress) + length;
            for (std::size_t rva = (static_cast<std::size_t>(s.VirtualAddress) + 7) & ~std::size_t{7};
                rva + 8 <= end; rva += 8) {
                std::uint32_t method = 0;
                if (Pointer(Read<std::uint64_t>(image.bytes + rva), method))
                    pointers.emplace_back(static_cast<DWORD>(rva), method);
            }
        }
        std::sort(pointers.begin(), pointers.end());
        pointers.erase(std::unique(pointers.begin(), pointers.end()), pointers.end());
        return true;
    }

    bool Match(DWORD rva, const NativeEntryFingerprint& pattern)
    {
        if (!pattern.length || pattern.length > 65536 || !image.Section(rva, pattern.length, true)) return false;
        const auto extent = extents.find(rva);
        if (pattern.unwind ? extent == extents.end() || extent->second != pattern.length : extent != extents.end())
            return false;
        const auto key = (static_cast<std::uint64_t>(rva) << 32) | pattern.length;
        auto it = hashes.find(key);
        if (it == hashes.end()) {
            std::array<unsigned char, 32> digest{};
            if (!HashBytes(image.bytes + rva, pattern.length, digest)) return false;
            it = hashes.emplace(key, digest).first;
        }
        return std::equal(it->second.begin(), it->second.end(), pattern.sha256);
    }
};

bool SameContract(const NativeAbiDescriptor& a, const NativeAbiDescriptor& b,
    const NativeAbiLayout& x, const NativeAbiLayout& y)
{
    return a.actions_table == b.actions_table && a.window_table == b.window_table &&
        a.lookup == b.lookup && a.accessor_rule == b.accessor_rule &&
        std::equal(std::begin(a.methods), std::end(a.methods), std::begin(b.methods)) &&
        x.actions == y.actions && x.tabs == y.tabs && x.active == y.active;
}

} // namespace

bool RecognizeNativeAbi(const unsigned char* bytes, std::size_t size, std::uintptr_t base,
    NativeAbiDescriptor& descriptor, NativeAbiLayout& layout)
{
    descriptor = {};
    layout = {};
    NativeImageView image{bytes, size};
    NativeImageIdentity identity;
    if (size > 64 * 1024 * 1024 || !image.Open() || !InspectNativeImage(bytes, size, identity)) return false;
    if (!base) base = static_cast<std::uintptr_t>(image.nt->OptionalHeader.ImageBase);
    PatternScan scan{image, base};
    if (!scan.Prepare()) return false;
    std::size_t family_count = 0;
    const auto families = BuiltinNativeAbiFamilies(family_count);
    NativeAbiDescriptor accepted;
    NativeAbiLayout accepted_layout;
    bool found = false;
    for (const auto& p : scan.pointers) {
        // Every table has a function pointer in slot 8; no fixture address or
        // version hint is needed to nominate it as the data-accessor candidate.
        if (p.first < 8 * 8) continue;
        const auto extent = scan.extents.find(p.second);
        if (extent == scan.extents.end()) continue;
        for (std::size_t f = 0; f < family_count; ++f) {
            const auto& family = families[f];
            if (extent->second != family.accessor_length) continue;
            NativeAbiDescriptor candidate;
            candidate.image = identity;
            candidate.actions_table = p.first - 8 * 8;
            candidate.accessor_rule = family.rule;
            if (!image.Section(candidate.actions_table, 13 * 8, false)) continue;
            bool entries_valid = true;
            for (unsigned slot = 0; slot < 13; ++slot) {
                if (!scan.Pointer(Read<std::uint64_t>(bytes + candidate.actions_table + slot * 8),
                    candidate.methods[slot])) { entries_valid = false; break; }
            }
            if (!entries_valid || family.lookup_operand + 4 > extent->second) continue;
            const auto call = static_cast<std::int64_t>(p.second) + family.lookup_operand + 4;
            const auto target = call + Read<std::int32_t>(bytes + p.second + family.lookup_operand);
            if (target < 0 || target > MAXDWORD) continue;
            candidate.lookup = static_cast<DWORD>(target);
            NativeAbiLayout decoded;
            if (!detail::CheckAccessor(image, candidate, decoded)) continue;
            for (std::size_t v = 0; v < family.count; ++v) {
                const auto& variant = family.variants[v];
                bool matches = true;
                for (unsigned slot = 0; slot < 13; ++slot) {
                    if (slot != 8 && !scan.Match(candidate.methods[slot], variant.entries[slot])) {
                        matches = false; break;
                    }
                }
                if (!matches) continue;
                DWORD window_table = 0;
                for (const auto& w : scan.pointers) {
                    if (!scan.Match(w.second, variant.window_entry)) continue;
                    if (window_table && window_table != w.first) return false;
                    window_table = w.first;
                }
                if (!window_table) continue;
                candidate.window_table = window_table;
                if (found && !SameContract(accepted, candidate, accepted_layout, decoded)) return false;
                accepted = candidate;
                accepted_layout = decoded;
                found = true;
            }
        }
    }
    if (!found) return false;
    descriptor = accepted;
    layout = accepted_layout;
    return true;
}

} // namespace quickky

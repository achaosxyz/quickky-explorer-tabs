#include "explorer/native_abi.h"
#include "explorer/abi_profiles/10.0.26100.9949.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <psapi.h>
#include <vector>

namespace {
unsigned failures = 0;
void Check(bool success, const char* message)
{
    if (!success) { ++failures; std::cerr << "FAIL: " << message << '\n'; }
}
template<class T> T Read(const unsigned char* p)
{ T value{}; std::memcpy(&value, p, sizeof(value)); return value; }
template<class T> void Write(unsigned char* p, T value)
{ std::memcpy(p, &value, sizeof(value)); }

const quickky::NativeAbiDescriptor& BaselineAbi()
{ return quickky::kExplorerFrame_10_0_26100_9949.descriptor; }

// Expand an OS file into a private RVA-addressed buffer. This fixture loader
// never executes image code or requires symbols, Explorer attachment, or a worker.
bool LoadImage(const std::wstring& path, std::vector<unsigned char>& image)
{
    image.clear();
    const auto file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    DWORD read = 0;
    std::vector<unsigned char> bytes;
    if (GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= 64 * 1024 * 1024) {
        bytes.resize(static_cast<std::size_t>(size.QuadPart));
        if (!ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read, nullptr) ||
            read != bytes.size()) bytes.clear();
    }
    CloseHandle(file);
    if (bytes.size() < sizeof(IMAGE_DOS_HEADER)) return false;
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<std::size_t>(dos->e_lfanew) > bytes.size() - sizeof(IMAGE_DOS_HEADER) ||
        bytes.size() - dos->e_lfanew < sizeof(IMAGE_NT_HEADERS64)) return false;
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data() + dos->e_lfanew);
    const auto section_offset = static_cast<std::size_t>(dos->e_lfanew) + sizeof(*nt);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER64) ||
        nt->OptionalHeader.SizeOfImage > 64 * 1024 * 1024 || !nt->OptionalHeader.SizeOfImage ||
        nt->OptionalHeader.SizeOfHeaders > bytes.size() ||
        nt->OptionalHeader.SizeOfHeaders > nt->OptionalHeader.SizeOfImage ||
        nt->FileHeader.NumberOfSections > (bytes.size() - section_offset) / sizeof(IMAGE_SECTION_HEADER)) return false;
    std::vector<unsigned char> expanded(nt->OptionalHeader.SizeOfImage);
    std::copy_n(bytes.begin(), nt->OptionalHeader.SizeOfHeaders, expanded.begin());
    const auto sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(bytes.data() + section_offset);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const auto& section = sections[i];
        if (section.PointerToRawData > bytes.size() || section.SizeOfRawData > bytes.size() - section.PointerToRawData ||
            section.VirtualAddress > expanded.size() || section.SizeOfRawData > expanded.size() - section.VirtualAddress) return false;
        std::copy_n(bytes.begin() + section.PointerToRawData, section.SizeOfRawData, expanded.begin() + section.VirtualAddress);
    }
    image.swap(expanded);
    return true;
}

IMAGE_NT_HEADERS64* Headers(std::vector<unsigned char>& image)
{
    return reinterpret_cast<IMAGE_NT_HEADERS64*>(image.data() +
        reinterpret_cast<IMAGE_DOS_HEADER*>(image.data())->e_lfanew);
}

bool Valid(const std::vector<unsigned char>& image, const quickky::NativeAbiDescriptor& descriptor)
{
    quickky::NativeAbiLayout layout;
    return quickky::ValidateNativeAbi(image.data(), image.size(), 0, descriptor, layout);
}

void ChangeIdentity(std::vector<unsigned char>& image, const quickky::NativeImageIdentity& identity)
{
    const auto d = Headers(image)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    auto entries = reinterpret_cast<IMAGE_DEBUG_DIRECTORY*>(image.data() + d.VirtualAddress);
    for (unsigned i = 0; i < d.Size / sizeof(*entries); ++i) {
        if (entries[i].Type != IMAGE_DEBUG_TYPE_CODEVIEW) continue;
        Write(image.data() + entries[i].AddressOfRawData + 4, identity.pdb);
        Write(image.data() + entries[i].AddressOfRawData + 20, identity.age);
    }
}

void CheckCatalogSelection(const std::vector<unsigned char>& original)
{
    std::size_t count = 0;
    const auto catalog = quickky::BuiltinNativeAbiProfiles(count);
    quickky::NativeAbiDescriptor selected;
    quickky::NativeAbiLayout layout;
    Check(quickky::SelectNativeAbi(original.data(), original.size(), 0, catalog, count, selected, layout),
        "select an actual image from the offline catalog");

    quickky::NativeAbiProfile candidates[]{
        {"9999.0.0.0", BaselineAbi()}, {"unknown-rule", BaselineAbi()},
        {"invalid-locator", BaselineAbi()}, {"version-label-is-not-a-filter", BaselineAbi()}};
    ++candidates[0].descriptor.image.age;
    candidates[1].descriptor.accessor_rule = MAXDWORD;
    candidates[2].descriptor.actions_table = MAXDWORD;
    Check(!Valid(original, candidates[1].descriptor), "reject unknown accessor rules");
    Check(quickky::SelectNativeAbi(original.data(), original.size(), 0, candidates, 4, selected, layout) &&
        layout.actions == 0xe8 && selected.actions_table == BaselineAbi().actions_table,
        "skip mismatched identity, rule and locator before a valid later map");
    std::reverse(candidates, candidates + 4);
    Check(quickky::SelectNativeAbi(original.data(), original.size(), 0, candidates, 4, selected, layout),
        "valid map selection is independent of catalog order and version labels");

    // These are synthetic identity changes, not evidence for another OS build.
    auto image = original;
    auto future = candidates[0];
    future.version = "synthetic-next-map";
    future.descriptor.image.pdb.Data1 ^= 0x10;
    ++future.descriptor.image.age;
    ChangeIdentity(image, future.descriptor.image);
    Check(!quickky::SelectNativeAbi(image.data(), image.size(), 0, catalog, count, selected, layout) &&
        !selected.accessor_rule && !layout.actions && !layout.tabs && !layout.active,
        "unknown identity fails closed and clears outputs without a nearest-version fallback");
    quickky::NativeAbiProfile extended[]{candidates[0], future};
    Check(quickky::SelectNativeAbi(image.data(), image.size(), 0, extended, 2, selected, layout) &&
        selected.image.age == future.descriptor.image.age && layout.actions == 0xe8,
        "a separately registered map can reuse the reviewed accessor rule");
    ++future.descriptor.image.size;
    Check(!quickky::SelectNativeAbi(image.data(), image.size(), 0, &future, 1, selected, layout),
        "matching GUID and age do not override an image-size mismatch");
    Check(!quickky::SelectNativeAbi(original.data(), original.size(), 0, nullptr, 0, selected, layout) &&
        !selected.accessor_rule && !layout.actions, "empty catalog fails closed");
}

void RejectMalformedImages()
{
    quickky::NativeImageIdentity identity;
    for (unsigned length : {0u, 1u, 63u, 64u, 512u, 4096u}) {
        std::vector<unsigned char> bytes(length);
        Check(!quickky::InspectNativeImage(bytes.data(), bytes.size(), identity), "reject truncated/zero PE");
        Check(!Valid(bytes, BaselineAbi()), "reject malformed descriptor/image pair");
    }
    std::vector<unsigned char> bytes(4096);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(bytes.data());
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = MAXLONG;
    Check(!quickky::InspectNativeImage(bytes.data(), bytes.size(), identity), "reject overflowing NT offset");
}

// A synthetic metadata-only image exercises identity admission on every host.
// It contains no native instructions or private entry points.
std::vector<unsigned char> IdentityImage()
{
    std::vector<unsigned char> image(0x2000);
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image.data());
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x80;
    const auto nt = Headers(image);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_AMD64;
    nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER64);
    nt->FileHeader.NumberOfSections = 1;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    nt->OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
    nt->OptionalHeader.SizeOfImage = static_cast<DWORD>(image.size());
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG] = {0x1000, sizeof(IMAGE_DEBUG_DIRECTORY)};
    auto section = reinterpret_cast<IMAGE_SECTION_HEADER*>(reinterpret_cast<unsigned char*>(nt) + sizeof(*nt));
    section->VirtualAddress = 0x1000;
    section->Misc.VirtualSize = 0x1000;
    section->Characteristics = IMAGE_SCN_MEM_READ;
    auto entry = reinterpret_cast<IMAGE_DEBUG_DIRECTORY*>(image.data() + 0x1000);
    entry->Type = IMAGE_DEBUG_TYPE_CODEVIEW;
    entry->SizeOfData = 25;
    entry->AddressOfRawData = 0x1100;
    std::memcpy(image.data() + 0x1100, "RSDS", 4);
    Write(image.data() + 0x1104, BaselineAbi().image.pdb);
    Write<DWORD>(image.data() + 0x1114, 1);
    return image;
}

void CheckImageIdentity()
{
    const auto original = IdentityImage();
    quickky::NativeImageIdentity identity;
    Check(quickky::InspectNativeImage(original.data(), original.size(), identity) &&
        InlineIsEqualGUID(identity.pdb, BaselineAbi().image.pdb) && identity.age == 1 && identity.size == original.size(),
        "inspect an unambiguous synthetic CodeView identity on every host");
    const auto reject = [&](const std::vector<unsigned char>& image, const char* message) {
        identity = BaselineAbi().image;
        Check(!quickky::InspectNativeImage(image.data(), image.size(), identity), message);
        Check(InlineIsEqualGUID(identity.pdb, GUID{}) && !identity.age && !identity.size,
            "failed identity inspection clears all outputs");
    };
    auto image = original;
    Write<DWORD>(image.data() + 0x1114, 0);
    reject(image, "reject a zero CodeView age");
    image = original;
    Write(image.data() + 0x1104, GUID{});
    reject(image, "reject a zero CodeView GUID");
    image = original;
    std::memcpy(image.data() + 0x1100, "NB10", 4);
    reject(image, "reject an unknown CodeView record shape");
    image = original;
    auto entries = reinterpret_cast<IMAGE_DEBUG_DIRECTORY*>(image.data() + 0x1000);
    entries->SizeOfData = 23;
    reject(image, "reject a truncated CodeView record");
    image = original;
    entries = reinterpret_cast<IMAGE_DEBUG_DIRECTORY*>(image.data() + 0x1000);
    entries[1] = entries[0];
    entries[1].AddressOfRawData = 0x1140;
    std::copy_n(image.data() + 0x1100, 25, image.data() + 0x1140);
    Headers(image)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].Size = 2 * sizeof(*entries);
    reject(image, "reject duplicate valid CodeView records");
    Write<DWORD>(image.data() + 0x1114, 0);
    reject(image, "an invalid first CodeView record cannot be bypassed by a valid second record");
    Write<DWORD>(image.data() + 0x1114, 1);
    Write<DWORD>(image.data() + 0x1154, 0);
    reject(image, "reject a valid CodeView record followed by an invalid record");
    image = original;
    Headers(image)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress = 0x1ff0;
    reject(image, "reject a debug directory outside the image");
}

void CheckNativeMemory()
{
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const auto page = system.dwPageSize;
    auto bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Check(bytes != nullptr, "allocate test-owned native-storage pages");
    if (!bytes) return;
    Check(!quickky::IsNativeMemoryReadable(nullptr, 1) && !quickky::IsNativeMemoryReadable(bytes, 0),
        "reject null and empty native-storage ranges");
    const struct { DWORD protection; bool readable; } cases[]{
        {PAGE_READONLY, true}, {PAGE_READWRITE, true}, {PAGE_EXECUTE, false},
        {PAGE_EXECUTE_READ, true}, {PAGE_EXECUTE_READWRITE, true},
        {PAGE_NOACCESS, false}, {PAGE_READWRITE | PAGE_GUARD, false}};
    DWORD previous = 0;
    for (const auto& test : cases) {
        const bool changed = VirtualProtect(bytes, 2 * page, test.protection, &previous) != FALSE;
        Check(changed, "set test-owned storage protection");
        if (changed) Check(quickky::IsNativeMemoryReadable(bytes, 2 * page) == test.readable,
            "native reads require readable protection, not just a committed or executable page");
    }
    const bool first = VirtualProtect(bytes, page, PAGE_READWRITE, &previous) != FALSE;
    const bool second = VirtualProtect(bytes + page, page, PAGE_NOACCESS, &previous) != FALSE;
    Check(first && second, "create a test-owned memory-region boundary");
    if (first && second) Check(quickky::IsNativeMemoryReadable(bytes, page) &&
        !quickky::IsNativeMemoryReadable(bytes + page - 1, 2), "native ranges cannot cross into an inaccessible region");
    Check(VirtualFree(bytes + page, page, MEM_DECOMMIT) != FALSE, "decommit test-owned storage");
    Check(!quickky::IsNativeMemoryReadable(bytes + page, 1), "reserved storage is not readable");
    Check(VirtualFree(bytes, 0, MEM_RELEASE) != FALSE, "release test-owned storage");
    const auto mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, page, nullptr);
    const auto copy = mapping ? MapViewOfFile(mapping, FILE_MAP_COPY, 0, 0, page) : nullptr;
    Check(copy && quickky::IsNativeMemoryReadable(copy, page), "copy-on-write storage is readable");
    if (copy) UnmapViewOfFile(copy);
    if (mapping) CloseHandle(mapping);
}

// Relocate accessor bodies in a private byte buffer, never in a loaded module.
// External rel32 references are adjusted; object displacements are deliberately
// preserved. This models a new link layout without executing private methods.
void MoveFunction(std::vector<unsigned char>& image, DWORD old_rva, DWORD new_rva,
    unsigned size, std::initializer_list<unsigned> relative_operands)
{
    std::vector<unsigned char> bytes(image.begin() + old_rva, image.begin() + old_rva + size);
    for (auto operand : relative_operands) {
        const auto delta = Read<std::int32_t>(bytes.data() + operand);
        Write(bytes.data() + operand, static_cast<std::int32_t>(delta + old_rva - new_rva));
    }
    std::copy(bytes.begin(), bytes.end(), image.begin() + new_rva);
    const auto d = Headers(image)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    auto functions = reinterpret_cast<RUNTIME_FUNCTION*>(image.data() + d.VirtualAddress);
    for (unsigned i = 0; i < d.Size / sizeof(*functions); ++i) {
        if (functions[i].BeginAddress == old_rva) {
            functions[i].BeginAddress = new_rva;
            functions[i].EndAddress = new_rva + size;
            return;
        }
    }
    Check(false, "fixture has an unwind entry for the moved accessor");
}

void CheckVerifiedImage(const std::vector<unsigned char>& original)
{
    auto descriptor = BaselineAbi();
    quickky::NativeAbiLayout layout;
    Check(quickky::ValidateNativeAbi(original.data(), original.size(), 0, descriptor, layout), "offline accessor contract");
    Check(layout.actions == 0xe8 && layout.tabs == 0x3590 && layout.active == 0x35a8, "decode verified layout");

    auto image = original;
    for (const auto offset : {0x1ed, 0x20d, 0x238, 0x27c}) {
        Write<DWORD>(image.data() + descriptor.methods[8] + offset, MAXDWORD - offset);
        Check(Valid(image, descriptor), "diagnostic source-line immediates do not veto the accessor contract");
        auto changed_instruction = image;
        changed_instruction[descriptor.methods[8] + offset - 1] ^= 1;
        Check(!Valid(changed_instruction, descriptor), "diagnostic instruction opcodes remain fingerprinted");
        changed_instruction = image;
        changed_instruction[descriptor.methods[8] + offset + 4] ^= 1;
        Check(!Valid(changed_instruction, descriptor), "diagnostic call opcodes remain fingerprinted");
    }
    image = original;
    auto d = Headers(image)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    auto entries = reinterpret_cast<IMAGE_DEBUG_DIRECTORY*>(image.data() + d.VirtualAddress);
    for (unsigned i = 0; i < d.Size / sizeof(*entries); ++i) {
        if (entries[i].Type == IMAGE_DEBUG_TYPE_CODEVIEW) {
            descriptor.image.pdb.Data1 ^= 0x10;
            Write(image.data() + entries[i].AddressOfRawData + 4, descriptor.image.pdb);
        }
    }
    Check(Valid(image, descriptor), "a different CodeView identity is not a compatibility veto");
    Check(!Valid(image, BaselineAbi()), "reject a descriptor for another image");

    Write<std::int32_t>(image.data() + descriptor.methods[8] + 0x4a, -0xf8);
    Write<DWORD>(image.data() + descriptor.lookup + 0x15, 0x4000);
    Write<DWORD>(image.data() + descriptor.lookup + 0x1c, 0x4008);
    Write<DWORD>(image.data() + descriptor.methods[8] + 0x9b, 0x3f20);
    Check(quickky::ValidateNativeAbi(image.data(), image.size(), 0, descriptor, layout) &&
        layout.actions == 0xf8 && layout.tabs == 0x3f08 && layout.active == 0x3f20, "decode changed object offsets");
    Write<DWORD>(image.data() + descriptor.lookup + 0x1c, 0x4010);
    Check(!Valid(image, descriptor), "reject inconsistent vector fields");

    image = original;
    descriptor = BaselineAbi();
    image[descriptor.lookup + 0xd6] ^= 1; // The tab record stride must remain 40.
    Check(!Valid(image, descriptor), "reject changed tab record contract");
    image = original;
    image[descriptor.methods[8] + 0x91] ^= 1;
    Check(!Valid(image, descriptor), "reject changed output/accessor instructions");
    image = original;
    Write<DWORD>(image.data() + descriptor.methods[8] + 0x60, 0);
    Check(!Valid(image, descriptor), "reject a redirected lookup call despite normalized fingerprint");
    image = original;
    descriptor.methods[3] = descriptor.methods[4];
    Check(!Valid(image, descriptor), "reject method slot mismatch");
    descriptor = BaselineAbi();
    descriptor.actions_table = MAXDWORD - 3;
    Check(!Valid(image, descriptor), "reject out-of-range vtable");
    descriptor = BaselineAbi();
    Headers(image)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size = MAXDWORD;
    Check(!Valid(image, descriptor), "reject malformed unwind directory");

    image = original;
    const auto old_data = descriptor.methods[8], old_lookup = descriptor.lookup;
    MoveFunction(image, old_data, 0x1000, 0x2cd, {0x16,0x60,0x7f,0xab,0xb7,0xc2,0xe8,
        0xf2,0xfd,0x12d,0x14a,0x177,0x194,0x19f,0x1a9,0x1bb,0x1e8,0x1f2,0x208,
        0x212,0x21d,0x233,0x23d,0x248,0x253,0x25e,0x277,0x281,0x28d,0x298,0x2a3,0x2ae,0x2b9});
    MoveFunction(image, old_lookup, 0x2000, 0xe3, {0x5b,0x73,0x92,0xac});
    descriptor.methods[8] = 0x1000;
    descriptor.lookup = 0x2000;
    Write<std::int32_t>(image.data() + 0x1060, 0x2000 - 0x1064);
    Write<std::uint64_t>(image.data() + descriptor.actions_table + 8 * 8,
        Headers(image)->OptionalHeader.ImageBase + 0x1000);
    Check(Valid(image, descriptor), "accept relocated accessors with matching contracts and RVAs");
    const quickky::NativeAbiProfile relocated[]{
        {"old-locators", BaselineAbi()}, {"synthetic-relink", descriptor}};
    quickky::NativeAbiDescriptor selected;
    Check(quickky::SelectNativeAbi(image.data(), image.size(), 0, relocated, 2, selected, layout) &&
        selected.methods[8] == 0x1000 && selected.lookup == 0x2000,
        "a relinked map is selected only after its updated locators pass the shared rule");
}
} // namespace

int wmain(int argc, wchar_t**)
{
    if (argc != 1) return 160;
    RejectMalformedImages();
    CheckImageIdentity();
    CheckNativeMemory();
    std::size_t count = 0;
    const auto catalog = quickky::BuiltinNativeAbiProfiles(count);
    Check(catalog && count && catalog[0].version && *catalog[0].version,
        "the built-in catalog contains a version-organized map");
    wchar_t system[MAX_PATH + 1]{};
    const auto length = GetSystemDirectoryW(system, MAX_PATH + 1);
    if (!length || length > MAX_PATH) return 1;
    const auto path = std::wstring(system) + L"\\ExplorerFrame.dll";
    std::vector<unsigned char> image;
    Check(LoadImage(path, image), "read system image without loading/executing it");
    if (image.empty()) return 1;
    quickky::NativeImageIdentity identity;
    Check(quickky::InspectNativeImage(image.data(), image.size(), identity), "inspect actual image identity");
    const auto& offline = BaselineAbi();
    if (InlineIsEqualGUID(identity.pdb, offline.image.pdb) && identity.age == offline.image.age) {
        CheckCatalogSelection(image);
        CheckVerifiedImage(image);
        // This isolated test process never calls into or reuses the uninitialized
        // module. Check loader relocation only; do not run DLL initialization.
        const auto module = LoadLibraryExW(path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
        MODULEINFO information{};
        quickky::NativeAbiLayout loaded_layout;
        Check(module && GetModuleInformation(GetCurrentProcess(), module, &information, sizeof(information)) &&
            quickky::ValidateNativeAbi(static_cast<unsigned char*>(information.lpBaseOfDll), information.SizeOfImage,
                reinterpret_cast<std::uintptr_t>(information.lpBaseOfDll), offline, loaded_layout),
            "validate actual loader relocation without DLL initialization or native calls");
        if (module) FreeLibrary(module);
        std::cout << "Verified-image layout and relocation cases exercised.\n";
    } else {
        Check(!Valid(image, offline), "unknown host cannot use the offline descriptor");
        quickky::NativeAbiDescriptor selected;
        quickky::NativeAbiLayout layout;
        bool registered = false;
        for (std::size_t i = 0; i < count; ++i) {
            const auto& candidate = catalog[i].descriptor.image;
            if (InlineIsEqualGUID(identity.pdb, candidate.pdb) && identity.age == candidate.age &&
                identity.size == candidate.size) registered = true;
        }
        if (!registered) Check(!quickky::SelectNativeAbi(image.data(), image.size(), 0, catalog, count, selected, layout),
            "an unregistered host cannot use a built-in map");
        std::cout << "Host differs from offline fixture; positive accessor cases unavailable.\n";
    }
    std::cout << "Native ABI failures: " << failures << '\n';
    return failures ? 1 : 0;
}

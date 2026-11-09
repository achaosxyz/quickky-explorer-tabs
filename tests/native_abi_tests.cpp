#include "abi_reference/profile.h"
#include "explorer/native_abi.h"

#include <algorithm>
#include <cstring>
#include <cwchar>
#include <iostream>
#include <psapi.h>
#include <winver.h>
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
{ return quickky::ExplorerFrameAbi26100_9549().descriptor; }

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

void CheckModeRecognition(const std::vector<unsigned char>& original,
    const quickky::NativeAbiProfile* oracle, bool unsupported)
{
    quickky::NativeAbiDescriptor selected;
    quickky::NativeAbiLayout layout;
    const auto recognized = quickky::RecognizeNativeAbi(original.data(), original.size(), 0, selected, layout);
    if (unsupported) {
        Check(!recognized && !selected.accessor_rule && !layout.actions,
            "excluded or missing-interface reference fails closed under mode recognition");
        return;
    }
    if (!oracle) return; // No claim that the local machine is a reviewed fixture.
    Check(recognized, "recognize reference code without fixture identity, version or locators as input");
    if (!recognized) return;
    std::cout << "Mode recognition exercised: rule " << selected.accessor_rule << '\n';
    quickky::NativeAbiLayout expected;
    const auto& abi = oracle->descriptor;
    Check(quickky::ValidateNativeAbi(original.data(), original.size(), 0, abi, expected), "independent fixture oracle");
    Check(selected.window_table == abi.window_table && selected.actions_table == abi.actions_table &&
        selected.lookup == abi.lookup && selected.accessor_rule == abi.accessor_rule &&
        std::equal(std::begin(selected.methods), std::end(selected.methods), std::begin(abi.methods)) &&
        layout.actions == expected.actions && layout.tabs == expected.tabs && layout.active == expected.active,
        "discovered entries, both tables, lookup and fields equal the independent oracle");
    auto image = original;
    auto identity = abi.image;
    identity.pdb.Data1 ^= 0x13579bdf;
    ++identity.age;
    ChangeIdentity(image, identity);
    Headers(image)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE] = {};
    Check(quickky::RecognizeNativeAbi(image.data(), image.size(), 0, selected, layout) &&
        InlineIsEqualGUID(selected.image.pdb, identity.pdb) && selected.image.age == identity.age,
        "mode recognition ignores unknown identity and missing version resource");
    // Exhaustive mutation cases run once, not once per identical method family.
    if (!InlineIsEqualGUID(abi.image.pdb, BaselineAbi().image.pdb)) return;
    for (unsigned slot = 0; slot < 13; ++slot) {
        image = original;
        image[abi.methods[slot]] ^= 1;
        Check(!quickky::RecognizeNativeAbi(image.data(), image.size(), 0, selected, layout) &&
            !selected.actions_table && !layout.active, "every typed entry remains mandatory");
    }
    image = original;
    Headers(image)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION].Size = MAXDWORD;
    Check(!quickky::RecognizeNativeAbi(image.data(), image.size(), 0, selected, layout),
        "mode scanning rejects an out-of-bounds unwind directory");
    image = original;
    Write<DWORD>(image.data() + abi.methods[8] + 0x60, 0);
    Check(!quickky::RecognizeNativeAbi(image.data(), image.size(), 0, selected, layout),
        "normalized lookup call still has to resolve to the reviewed lookup contract");
    image = original;
    const auto window_entry = Read<std::uint64_t>(image.data() + abi.window_table) -
        Headers(image)->OptionalHeader.ImageBase;
    image[static_cast<std::size_t>(window_entry) + 15] ^= 1;
    Check(!quickky::RecognizeNativeAbi(image.data(), image.size(), 0, selected, layout),
        "window leaf fingerprint includes the complete return, not just a prefix");
    image = original;
    const auto old_size = image.size();
    image.resize(old_size + 0x1000);
    auto nt = Headers(image);
    auto section = IMAGE_FIRST_SECTION(nt) + nt->FileHeader.NumberOfSections;
    Check(reinterpret_cast<unsigned char*>(section + 1) <= image.data() + nt->OptionalHeader.SizeOfHeaders,
        "synthetic readonly section fits the existing header allocation");
    *section = {};
    section->VirtualAddress = static_cast<DWORD>(old_size);
    section->Misc.VirtualSize = section->SizeOfRawData = 0x1000;
    section->Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA;
    ++nt->FileHeader.NumberOfSections;
    nt->OptionalHeader.SizeOfImage = static_cast<DWORD>(image.size());
    auto unique = image;
    std::copy_n(original.data() + abi.actions_table, 13 * 8, image.data() + old_size);
    Check(!quickky::RecognizeNativeAbi(image.data(), image.size(), 0, selected, layout) && !layout.actions,
        "two valid actions tables are ambiguous, not first-match wins");
    image = unique;
    std::copy_n(original.data() + abi.window_table, 8, image.data() + old_size);
    Check(!quickky::RecognizeNativeAbi(image.data(), image.size(), 0, selected, layout),
        "two matching window-table pointers are ambiguous");
    image = unique;
    std::copy_n(original.data() + abi.actions_table, 13 * 8, image.data() + old_size);
    std::fill_n(image.data() + abi.actions_table, 13 * 8, static_cast<unsigned char>(0));
    std::copy_n(original.data() + abi.window_table, 8, image.data() + old_size + 0x100);
    std::fill_n(image.data() + abi.window_table, 8, static_cast<unsigned char>(0));
    Check(quickky::RecognizeNativeAbi(image.data(), image.size(), 0, selected, layout) &&
        selected.actions_table == old_size && selected.window_table == old_size + 0x100,
        "both tables relocate and image size changes without a new DLL map");

    // A small PE can otherwise expand into millions of tree nodes before the
    // readonly-section scan budget is checked. Retain real entries and append
    // unique one-byte ranges so contract recognition would still succeed without
    // an explicit exception-entry budget checked before index allocation.
    constexpr std::size_t excessive_count = 65537;
    const auto directory = Headers(image)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
    const auto original_count = directory.Size / sizeof(RUNTIME_FUNCTION);
    const auto extra_size = (excessive_count * sizeof(RUNTIME_FUNCTION) + 4095) & ~std::size_t{4095};
    image = original;
    image.resize(old_size + extra_size);
    nt = Headers(image);
    section = IMAGE_FIRST_SECTION(nt) + nt->FileHeader.NumberOfSections;
    *section = {};
    section->VirtualAddress = static_cast<DWORD>(old_size);
    section->Misc.VirtualSize = section->SizeOfRawData = static_cast<DWORD>(extra_size);
    section->Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA;
    ++nt->FileHeader.NumberOfSections;
    nt->OptionalHeader.SizeOfImage = static_cast<DWORD>(image.size());
    auto functions = reinterpret_cast<RUNTIME_FUNCTION*>(image.data() + old_size);
    std::copy_n(original.data() + directory.VirtualAddress, directory.Size, image.data() + old_size);
    std::vector<DWORD> starts;
    for (std::size_t i = 0; i < original_count; ++i) starts.push_back(functions[i].BeginAddress);
    for (const auto method : abi.methods) starts.push_back(method);
    starts.push_back(static_cast<DWORD>(window_entry));
    std::sort(starts.begin(), starts.end());
    const auto text = IMAGE_FIRST_SECTION(nt);
    DWORD next = text->VirtualAddress;
    for (std::size_t i = original_count; i < excessive_count; ++i) {
        while (std::binary_search(starts.begin(), starts.end(), next)) ++next;
        functions[i] = {next, next + 1, 0};
        ++next;
    }
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION] =
        {static_cast<DWORD>(old_size), static_cast<DWORD>(excessive_count * sizeof(RUNTIME_FUNCTION))};
    Check(!quickky::RecognizeNativeAbi(image.data(), image.size(), 0, selected, layout) && !layout.actions,
        "reject excessive exception entries before allocating the function index");

    // Repeated pointers need little image space but can expand the candidate
    // index or trigger repeated full accessor/lookup validation. Preserve the
    // valid original contract: accepting it after a partial scan is unsafe.
    for (const bool pointer_limit : {true, false}) {
        const std::size_t repeats = pointer_limit ? 65537 : 257;
        const auto added_size = (repeats * sizeof(std::uint64_t) + 4095) & ~std::size_t{4095};
        image = original;
        image.resize(old_size + added_size);
        nt = Headers(image);
        section = IMAGE_FIRST_SECTION(nt) + nt->FileHeader.NumberOfSections;
        *section = {};
        section->VirtualAddress = static_cast<DWORD>(old_size);
        section->Misc.VirtualSize = section->SizeOfRawData = static_cast<DWORD>(added_size);
        section->Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_CNT_INITIALIZED_DATA;
        ++nt->FileHeader.NumberOfSections;
        nt->OptionalHeader.SizeOfImage = static_cast<DWORD>(image.size());
        const auto method = abi.methods[pointer_limit ? 3 : 8];
        for (std::size_t i = 0; i < repeats; ++i)
            Write<std::uint64_t>(image.data() + old_size + i * sizeof(std::uint64_t),
                nt->OptionalHeader.ImageBase + method);
        Check(!quickky::RecognizeNativeAbi(image.data(), image.size(), 0, selected, layout) &&
            !selected.actions_table && !layout.actions, pointer_limit ?
            "reject excessive readonly code pointers before expanding the candidate index" :
            "reject excessive accessor candidates without accepting a partial uniqueness scan");
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

void CheckVersionResource()
{
    constexpr wchar_t key[] = L"VS_VERSION_INFO";
    constexpr auto offset = (6 + sizeof(key) + 3) & ~std::size_t(3);
    std::vector<unsigned char> bytes(offset + sizeof(VS_FIXEDFILEINFO));
    Write<WORD>(bytes.data(), static_cast<WORD>(bytes.size()));
    Write<WORD>(bytes.data() + 2, sizeof(VS_FIXEDFILEINFO));
    std::memcpy(bytes.data() + 6, key, sizeof(key));
    VS_FIXEDFILEINFO info{};
    info.dwSignature = 0xfeef04bd;
    info.dwStrucVersion = 0x00010000;
    info.dwFileVersionMS = (10u << 16);
    info.dwFileVersionLS = (26100u << 16) | 9549;
    Write(bytes.data() + offset, info);
    const auto version = quickky::PackNativeFileVersion(10, 0, 26100, 9549);
    Check(quickky::NativeFileVersionResource(bytes.data(), bytes.size()) == version,
        "read the fixed four-word component version");
    Check(!quickky::NativeFileVersionResource(nullptr, bytes.size()) &&
        !quickky::NativeModuleVersion(nullptr), "missing version metadata disables fallback");
    for (std::size_t size = 0; size < bytes.size(); ++size)
        Check(!quickky::NativeFileVersionResource(bytes.data(), size), "reject truncated version values");
    for (const auto site : {0u, 2u, 4u, 6u, static_cast<unsigned>(offset),
        static_cast<unsigned>(offset + 4)}) {
        auto changed = bytes;
        changed[site] ^= 0x80;
        Check(!quickky::NativeFileVersionResource(changed.data(), changed.size()),
            "reject malformed version root, key, type, signature and structure version");
    }
}

void CheckVersionMap(const std::vector<unsigned char>& original, const quickky::NativeAbiProfile& profile)
{
    std::size_t count = 0;
    const auto catalog = quickky::BuiltinNativeAbiProfiles(count);
    quickky::NativeAbiDescriptor selected;
    quickky::NativeAbiLayout layout;
    const quickky::NativeAbiProfile* matched = nullptr;
    Check(quickky::SelectNativeAbi(original.data(), original.size(), 0, catalog, count,
        selected, layout, 0, &matched) && matched == &profile,
        "each exact image selects its independently registered version unit");
    const bool older = profile.descriptor.accessor_rule == 2;
    const bool newer = profile.descriptor.accessor_rule >= 3;
    // Exact-image observations are test evidence, not production layout hints.
    // Even versions sharing an accessor rule can have different object fields.
    struct Expected { const char* version; DWORD tabs; DWORD active; };
    constexpr Expected observations[]{
        {"10.0.22621.5983", 0x1a0, 0x1b8},
        {"10.0.26100.1000", 0x198, 0x1b0},
        {"10.0.26100.712", 0x198, 0x1b0},
        {"10.0.26100.1150", 0x198, 0x1b0},
        {"10.0.26100.1455", 0x198, 0x1b0},
        {"10.0.26100.1591", 0x198, 0x1b0},
        {"10.0.26100.1882", 0x198, 0x1b0},
        {"10.0.26100.2161", 0x198, 0x1b0},
        {"10.0.26100.2314", 0x198, 0x1b0},
        {"10.0.26100.2454", 0x198, 0x1b0},
        {"10.0.26100.2605", 0x198, 0x1b0},
        {"10.0.26100.2894", 0x198, 0x1b0},
        {"10.0.26100.3037", 0x198, 0x1b0},
        {"10.0.26100.3323", 0x198, 0x1b0},
        {"10.0.26100.3624", 0x198, 0x1b0},
        {"10.0.26100.3912", 0x198, 0x1b0},
        {"10.0.26100.4061", 0x198, 0x1b0},
        {"10.0.26100.4484", 0x198, 0x1b0},
        {"10.0.26100.4202", 0x198, 0x1b0},
        {"10.0.26100.4768", 0x198, 0x1b0},
        {"10.0.22621.3958", 0x1a0, 0x1b8},
        {"10.0.22621.4541", 0x1a0, 0x1b8},
        {"10.0.22621.4601", 0x1a0, 0x1b8},
        {"10.0.22621.4746", 0x1a0, 0x1b8},
        {"10.0.26100.7309", 0x1a8, 0x1c0},
        {"10.0.26100.7462", 0x1a8, 0x1c0},
        {"10.0.26100.7705", 0x1a8, 0x1c0},
        {"10.0.26100.7824", 0x1a8, 0x1c0},
        {"10.0.26100.7920", 0x1a8, 0x1c0},
        {"10.0.26100.8115", 0x1a8, 0x1c0},
        {"10.0.26100.8328", 0x208, 0x220},
        {"10.0.26100.8457", 0x208, 0x220},
        {"10.0.26100.8521", 0x208, 0x220},
        {"10.0.22621.3672", 0x1a0, 0x1b8},
        {"10.0.22621.5909", 0x1a0, 0x1b8},
        {"10.0.22621.6197", 0x1a0, 0x1b8},
        {"10.0.26100.5074", 0x198, 0x1b0},
        {"10.0.26100.6725", 0x1a0, 0x1b8},
        {"10.0.26100.7019", 0x1a0, 0x1b8},
        {"10.0.26100.7171", 0x1a0, 0x1b8},
        {"10.0.22621.5413", 0x1a0, 0x1b8},
        {"10.0.22621.5331", 0x1a0, 0x1b8},
        {"10.0.22621.5262", 0x1a0, 0x1b8},
        {"10.0.26100.1", 0x198, 0x1b0},
        {"10.0.26100.8655", 0x208, 0x220},
        {"10.0.26100.8737", 0x1308, 0x1320},
        {"10.0.26100.8972", 0x1308, 0x1320},
        {"10.0.26100.9278", 0x1308, 0x1320},
        {"10.0.26100.9549", 0x3590, 0x35a8},
        {"10.0.26100.9818", 0x3590, 0x35a8},
        {"10.0.28000.2525", 0x258, 0x270},
        {"10.0.28000.1761", 0x1a8, 0x1c0},
        {"10.0.28000.2605", 0x1308, 0x1320},
        {"10.0.28000.3086", 0x1308, 0x1320}};
    const Expected* expected = nullptr;
    for (const auto& entry : observations)
        if (std::strcmp(entry.version, profile.version) == 0) expected = &entry;
    Check(expected != nullptr, "registered image has independently inspected layout evidence");
    if (!expected) return;
    const auto expected_tabs = expected->tabs;
    const auto expected_active = expected->active;
    Check(layout.actions == 0xe8 && layout.tabs == expected_tabs && layout.active == expected_active,
        "derive the layout for this real image");

    // Only the identity is synthetic. These cases prove gating, not live support
    // for a future Windows installation or transitive private-call behavior.
    auto image = original;
    auto identity = profile.descriptor.image;
    identity.pdb.Data1 ^= 0x100;
    ChangeIdentity(image, identity);
    const auto select = [&](const std::vector<unsigned char>& bytes,
        const quickky::NativeAbiProfile* maps, std::size_t size, std::uint64_t version) {
        return quickky::SelectNativeAbi(bytes.data(), bytes.size(), 0, maps, size,
            selected, layout, version, &matched);
    };
    Check(!select(image, catalog, count, 0) && !matched && !layout.actions && !selected.accessor_rule,
        "unknown identity without version metadata clears outputs and refuses fallback");
    Check(!select(image, &profile, 1, profile.file_version - 1), "never try a newer version map");
    Check(select(image, catalog, count, profile.file_version) && matched == &profile &&
        InlineIsEqualGUID(selected.image.pdb, identity.pdb),
        "same-version unknown identity requires all complete entry fingerprints");
    Check(select(image, catalog, count, profile.file_version + 1) && matched == &profile,
        "nearest-lower map binds after complete read-only validation");

    quickky::NativeAbiProfile candidates[]{profile, profile, profile};
    candidates[0].file_version -= 1;
    candidates[2].file_version += 1;
    candidates[2].descriptor.methods[0] = MAXDWORD;
    Check(select(image, candidates, 3, profile.file_version + 1) && matched == candidates + 1,
        "skip a broken nearest map, prefer the closest valid lower map regardless of registration order");
    candidates[1].descriptor.accessor_rule = MAXDWORD;
    Check(select(image, candidates, 3, profile.file_version + 1) && matched == candidates,
        "a fingerprint match with an invalid accessor rule cannot block another validated candidate");

    for (unsigned slot = 0; slot < 13; ++slot) {
        auto changed = image;
        const auto& entry = profile.entry_fingerprints[slot];
        changed[profile.descriptor.methods[slot] + entry.length - 1] ^= 1;
        Check(!select(changed, &profile, 1, profile.file_version + 1) && !matched &&
            !layout.actions && !selected.accessor_rule, "fallback rejects damage to every typed entry");
    }
    for (const auto site : profile.data_diagnostics) {
        auto changed = image;
        Write<DWORD>(changed.data() + profile.descriptor.methods[8] + site, MAXDWORD);
        Check(select(changed, &profile, 1, profile.file_version + 1),
            "fallback permits reviewed diagnostic source-line changes");
        changed[profile.descriptor.methods[8] + site - 1] ^= 1;
        Check(!select(changed, &profile, 1, profile.file_version + 1),
            "fallback does not mask diagnostic instruction opcodes");
    }
    auto changed = image;
    const auto active_operand = profile.descriptor.accessor_rule == 5 ? 0x117u :
        profile.descriptor.accessor_rule == 4 ? 0xa2u :
        older ? 0xacu : newer ? 0x8eu : 0x9bu;
    const auto active_site = profile.descriptor.methods[8] + active_operand;
    Write<DWORD>(changed.data() + active_site, expected_active + 8);
    auto rebound = profile.descriptor;
    rebound.image = identity;
    Check(Valid(changed, rebound) && !select(changed, &profile, 1, profile.file_version + 1),
        "fallback requires unchanged fields even when the accessor family could derive them");
    changed = image;
    Write<DWORD>(changed.data() + profile.descriptor.methods[8] +
        (profile.descriptor.accessor_rule == 5 ? 0x86u : newer ? 0x4au : 0x60u), 0);
    Check(!select(changed, &profile, 1, profile.file_version + 1), "fallback rejects redirected relative calls");
    candidates[0] = profile;
    candidates[0].descriptor.accessor_rule = MAXDWORD;
    candidates[1] = profile;
    candidates[1].descriptor.image = identity;
    Check(!select(original, candidates, 2, profile.file_version + 1),
        "a recognized identity with a failed contract cannot escape through unknown-identity fallback");
    if (profile.descriptor.accessor_rule == 5) {
        const auto lookup = profile.descriptor.lookup;
        std::int32_t displacement = 0;
        std::memcpy(&displacement, original.data() + lookup + 0x41, sizeof(displacement));
        const auto cold = static_cast<std::uint32_t>(lookup + 0x45 + displacement);
        changed = original;
        changed[cold + 3] = 0x20;
        Check(!Valid(changed, profile.descriptor), "split lookup rejects a changed record stride");
        changed = original;
        changed[cold + 5] ^= 1;
        Check(!Valid(changed, profile.descriptor), "split lookup rejects a redirected cold backedge");
        changed = original;
        changed[lookup + 0x41] ^= 1;
        Check(!Valid(changed, profile.descriptor), "split lookup rejects an unrecognized cold destination");
    }
    std::cout << "Version map and conservative fallback exercised: " << profile.version << '\n';
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
    quickky::NativeAbiDescriptor discovered;
    quickky::NativeAbiLayout discovered_layout;
    Check(quickky::RecognizeNativeAbi(image.data(), image.size(), 0, discovered, discovered_layout) &&
        discovered.methods[8] == 0x1000 && discovered.lookup == 0x2000,
        "discover relocated accessors without a supplied relinked map");
    const quickky::NativeAbiProfile relocated[]{
        {"old-locators", BaselineAbi()}, {"synthetic-relink", descriptor}};
    quickky::NativeAbiDescriptor selected;
    Check(quickky::SelectNativeAbi(image.data(), image.size(), 0, relocated, 2, selected, layout) &&
        selected.methods[8] == 0x1000 && selected.lookup == 0x2000,
        "a relinked map is selected only after its updated locators pass the shared rule");
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    const bool expect_unsupported = argc == 3 && !std::wcscmp(argv[1], L"--unsupported-image");
    if (argc != 1 && (argc != 3 || (!expect_unsupported && std::wcscmp(argv[1], L"--image")))) return 160;
    RejectMalformedImages();
    CheckImageIdentity();
    CheckNativeMemory();
    CheckVersionResource();
    std::size_t count = 0;
    const auto catalog = quickky::BuiltinNativeAbiProfiles(count);
    Check(catalog && count && catalog[0].version && *catalog[0].version,
        "the built-in catalog contains a version-organized map");
    wchar_t system[MAX_PATH + 1]{};
    const auto length = GetSystemDirectoryW(system, MAX_PATH + 1);
    if (!length || length > MAX_PATH) return 1;
    const auto path = argc == 3 ? std::wstring(argv[2]) : std::wstring(system) + L"\\ExplorerFrame.dll";
    std::vector<unsigned char> image;
    Check(LoadImage(path, image), "read system image without loading/executing it");
    if (image.empty()) return 1;
    quickky::NativeImageIdentity identity;
    Check(quickky::InspectNativeImage(image.data(), image.size(), identity), "inspect actual image identity");
    const quickky::NativeAbiProfile* registered = nullptr;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& candidate = catalog[i].descriptor.image;
        if (InlineIsEqualGUID(identity.pdb, candidate.pdb) && identity.age == candidate.age &&
            identity.size == candidate.size) registered = catalog + i;
    }
    if (argc == 3 && !expect_unsupported)
        Check(registered != nullptr, "explicit image fixture has a reviewed version map");
    CheckModeRecognition(image, registered, expect_unsupported);
    const auto previous_module = GetModuleHandleW(L"ExplorerFrame.dll");
    const auto report = quickky::InspectNativeAbiFile(path.c_str());
    Check(report.inspected && report.file_version != 0,
        "installed-file diagnostic reads the fixed resource without DLL initialization");
    Check(GetModuleHandleW(L"ExplorerFrame.dll") == previous_module,
        "image-resource diagnostic does not register a resolvable executable DLL");
    if (registered) Check(report.supported && report.file_version == registered->file_version,
        "installed-file diagnostic recognizes the reviewed version and code contract");
    const auto missing = quickky::InspectNativeAbiFile((path + L".qtabs-not-present").c_str());
    Check(!missing.inspected && !missing.supported && !missing.file_version,
        "missing installed file is an inspection failure, not a compatibility result");
    Check(!quickky::InspectNativeAbiFile(nullptr).inspected,
        "null diagnostic path fails without loading anything");
    if (expect_unsupported) {
        quickky::NativeAbiDescriptor selected;
        quickky::NativeAbiLayout layout;
        Check(!registered && !report.supported &&
            !quickky::SelectNativeAbi(image.data(), image.size(), 0, catalog, count,
                selected, layout, report.file_version) && !selected.accessor_rule && !layout.actions,
            "excluded accessor shapes and missing transfer interfaces remain unsupported");
        std::cout << "Unsupported-image failures: " << failures << '\n';
        return failures ? 1 : 0;
    }
    if (registered) {
        CheckVersionMap(image, *registered);
        // Check loader relocation and the loaded resource in this isolated
        // process. Do not initialize the image, attach to Explorer or call it.
        // Occupy the preferred address so the ASLR regression cannot silently
        // pass without exercising a relocated image on an otherwise empty host.
        const auto preferred = static_cast<std::uintptr_t>(Headers(image)->OptionalHeader.ImageBase);
        const auto reservation = VirtualAlloc(reinterpret_cast<void*>(preferred), 1, MEM_RESERVE, PAGE_NOACCESS);
        const auto module = LoadLibraryExW(path.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
        MODULEINFO information{};
        quickky::NativeAbiDescriptor selected;
        quickky::NativeAbiLayout loaded_layout;
        const auto version = quickky::NativeModuleVersion(module);
        if (version != registered->file_version)
            std::cerr << "Loaded version: " << std::hex << version << ", map version: "
                << registered->file_version << std::dec << '\n';
        Check(version == registered->file_version, "read version metadata from the actual loaded module");
        Check(module && GetModuleInformation(GetCurrentProcess(), module, &information, sizeof(information)) &&
            quickky::SelectNativeAbi(static_cast<unsigned char*>(information.lpBaseOfDll), information.SizeOfImage,
                reinterpret_cast<std::uintptr_t>(information.lpBaseOfDll), catalog, count, selected,
                loaded_layout, version), "validate actual loader relocation without initialization or native calls");
        if (module && information.lpBaseOfDll) {
            Check(reinterpret_cast<std::uintptr_t>(information.lpBaseOfDll) != preferred,
                "exercise diagnostic reuse with a genuinely relocated module");
            Check(quickky::RecognizeNativeAbi(static_cast<unsigned char*>(information.lpBaseOfDll),
                information.SizeOfImage, reinterpret_cast<std::uintptr_t>(information.lpBaseOfDll), selected, loaded_layout) &&
                selected.actions_table == registered->descriptor.actions_table &&
                selected.window_table == registered->descriptor.window_table,
                "mode recognition directly handles a nonpreferred actual ASLR base");
            const auto reused = quickky::InspectNativeAbiFile(path.c_str());
            Check(reused.inspected && reused.supported && reused.file_version == version,
                "diagnostic handles an already-loaded ASLR image without preferred-base false negatives");
            auto fallback = *registered;
            fallback.descriptor.image.pdb.Data1 ^= 0x100;
            Check(quickky::SelectNativeAbi(static_cast<unsigned char*>(information.lpBaseOfDll),
                information.SizeOfImage, reinterpret_cast<std::uintptr_t>(information.lpBaseOfDll),
                &fallback, 1, selected, loaded_layout, version),
                "complete fallback fingerprints also match the actual ASLR-relocated image");
        }
        if (module) FreeLibrary(module);
        if (reservation) Check(VirtualFree(reservation, 0, MEM_RELEASE) != FALSE,
            "release the test-owned preferred-base reservation");
    }
    const auto& offline = BaselineAbi();
    if (InlineIsEqualGUID(identity.pdb, offline.image.pdb) && identity.age == offline.image.age) {
        CheckCatalogSelection(image);
        CheckVerifiedImage(image);
        std::cout << "Verified-image layout and relocation cases exercised.\n";
    } else {
        Check(!Valid(image, offline), "unknown host cannot use the offline descriptor");
        quickky::NativeAbiDescriptor selected;
        quickky::NativeAbiLayout layout;
        if (!registered) Check(!quickky::SelectNativeAbi(image.data(), image.size(), 0, catalog, count, selected, layout),
            "an unregistered host cannot use a built-in map");
        std::cout << "Baseline-only mutation cases skipped for this image.\n";
    }
    std::cout << "Native ABI failures: " << failures << '\n';
    return failures ? 1 : 0;
}

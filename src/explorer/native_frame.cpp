#include "explorer/native_frame.h"

#include <algorithm>
#include <cstring>
#include <map>

namespace quickky {
namespace {
// ExplorerFrame 10.0.26100.9949. These offsets were checked against its exact
// public PDB and scoped native frames, not inferred from the Windows build name.
constexpr GUID kImageGuid{0x822a8c93, 0x960a, 0xc7b6, {0x7f,0x6a,0x87,0xcf,0x48,0xe8,0xce,0x6f}};
constexpr std::size_t kWindowTable = 0x27b2a0;
constexpr std::size_t kActionsTable = 0x27b090;
constexpr std::size_t kActionsOffset = 0xe8;
constexpr std::size_t kTabsOffset = 0x3590;
constexpr std::size_t kActiveOffset = 0x35a8;
constexpr GUID kNoTab{};
thread_local std::map<void*, std::weak_ptr<NativeFrame>> proxies;

bool Readable(const void* pointer, std::size_t size)
{
    MEMORY_BASIC_INFORMATION region{};
    const auto p = reinterpret_cast<std::uintptr_t>(pointer);
    return pointer && VirtualQuery(pointer, &region, sizeof(region)) &&
        region.State == MEM_COMMIT && !(region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
        p >= reinterpret_cast<std::uintptr_t>(region.BaseAddress) &&
        size <= region.RegionSize - (p - reinterpret_cast<std::uintptr_t>(region.BaseAddress));
}

bool MatchedImage(unsigned char* image)
{
    if (!image) return false;
    const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return false;
    const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return false;
    const auto directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    if (!directory.VirtualAddress || directory.Size > nt->OptionalHeader.SizeOfImage ||
        directory.VirtualAddress > nt->OptionalHeader.SizeOfImage - directory.Size) return false;
    const auto entries = reinterpret_cast<const IMAGE_DEBUG_DIRECTORY*>(image + directory.VirtualAddress);
    for (unsigned i = 0; i < directory.Size / sizeof(*entries); ++i) {
        const auto& entry = entries[i];
        if (entry.Type != IMAGE_DEBUG_TYPE_CODEVIEW || entry.SizeOfData < 24 ||
            entry.AddressOfRawData > nt->OptionalHeader.SizeOfImage - 24) continue;
        const auto bytes = image + entry.AddressOfRawData;
        GUID guid{}; DWORD age = 0;
        std::memcpy(&guid, bytes + 4, sizeof(guid));
        std::memcpy(&age, bytes + 20, sizeof(age));
        if (std::memcmp(bytes, "RSDS", 4) == 0 && InlineIsEqualGUID(guid, kImageGuid) && age == 1) return true;
    }
    return false;
}

struct BrowserTab {
    void* browser;
    void* site;
    GUID id;
    std::uint64_t flags;
};
static_assert(sizeof(BrowserTab) == 40, "The matched native tab record is x64-only.");

// GetTabDataById writes exactly these fields in the matched profile. Its native
// TabDataFree uses CoTaskMemFree for both outputs (verified in the image imports).
struct TabData {
    GUID id{};
    PWSTR title = nullptr;
    PIDLIST_ABSOLUTE folder = nullptr;
    ~TabData() { CoTaskMemFree(title); CoTaskMemFree(folder); }
};
static_assert(sizeof(TabData) == 32, "Matched FileExplorerTabData layout.");

std::shared_ptr<NativeFrame> FindProxy(void* self)
{
    const auto it = proxies.find(self);
    return it == proxies.end() ? nullptr : it->second.lock();
}
} // namespace

std::shared_ptr<NativeFrame> NativeFrame::Bind(HWND root)
{
    if (GetWindowThreadProcessId(root, nullptr) != GetCurrentThreadId()) return {};
    auto image = reinterpret_cast<unsigned char*>(GetModuleHandleW(L"ExplorerFrame.dll"));
    if (!MatchedImage(image)) return {};
    auto object = reinterpret_cast<unsigned char*>(GetWindowLongPtrW(root, 0));
    if (!Readable(object, kActionsOffset + kActiveOffset + sizeof(void*)) ||
        *reinterpret_cast<void**>(object) != image + kWindowTable ||
        *reinterpret_cast<void**>(object + kActionsOffset) != image + kActionsTable) return {};
    auto frame = std::shared_ptr<NativeFrame>(new NativeFrame);
    frame->root_ = root;
    frame->thread_ = GetCurrentThreadId();
    frame->object_ = object;
    frame->actions_ = object + kActionsOffset;
    frame->original_ = *reinterpret_cast<void***>(frame->actions_);
    reinterpret_cast<IUnknown*>(frame->actions_)->AddRef();
    return frame;
}

NativeFrame::~NativeFrame()
{
    StopPlus();
    if (actions_) reinterpret_cast<IUnknown*>(actions_)->Release();
}

bool NativeFrame::Valid() const
{
    return actions_ && thread_ == GetCurrentThreadId() && IsWindow(root_) &&
        reinterpret_cast<unsigned char*>(GetWindowLongPtrW(root_, 0)) == object_;
}

bool NativeFrame::ReadTabs(std::vector<GUID>& tabs, GUID& active) const
{
    tabs.clear(); active = {};
    if (!Valid()) return false;
    const auto bytes = static_cast<const unsigned char*>(actions_);
    const auto first = *reinterpret_cast<BrowserTab* const*>(bytes + kTabsOffset);
    const auto last = *reinterpret_cast<BrowserTab* const*>(bytes + kTabsOffset + sizeof(void*));
    const auto begin_address = reinterpret_cast<std::uintptr_t>(first);
    const auto end_address = reinterpret_cast<std::uintptr_t>(last);
    if (end_address < begin_address || (end_address - begin_address) % sizeof(BrowserTab) ||
        end_address - begin_address > kMaxNativeTabs * sizeof(BrowserTab)) return false;
    const auto count = (end_address - begin_address) / sizeof(BrowserTab);
    if (!count) return true;
    if (!Readable(first, count * sizeof(BrowserTab))) return false;
    const auto selected = *reinterpret_cast<void* const*>(bytes + kActiveOffset);
    for (std::size_t i = 0; i < count; ++i) {
        if (InlineIsEqualGUID(first[i].id, kNoTab)) return false;
        tabs.push_back(first[i].id);
        if (first[i].browser == selected) active = first[i].id;
    }
    return !InlineIsEqualGUID(active, kNoTab);
}

bool NativeFrame::Snapshot(std::vector<GUID>& tabs, GUID& active, FolderLocation& folder)
{
    if (!ReadTabs(tabs, active) || tabs.empty()) return false;
    TabData data;
    data.id = active;
    using Read = HRESULT (STDMETHODCALLTYPE*)(void*, TabData*);
    const auto result = reinterpret_cast<Read>(original_[8])(actions_, &data);
    // Read from the selected browser on its owning thread. Shell automation
    // enumeration is not a reliable callout from synchronous WM_COPYDATA input.
    std::vector<GUID> after;
    GUID still_active{};
    if (FAILED(result) || !data.folder || !ReadTabs(after, still_active) ||
        !InlineIsEqualGUID(active, still_active)) return false;
    folder.pidl.reset(data.folder);
    data.folder = nullptr;
    return true;
}

HRESULT NativeFrame::AddOriginal(PCIDLIST_ABSOLUTE folder, int entry, GUID* created)
{
    using Function = HRESULT (STDMETHODCALLTYPE*)(void*, PCIDLIST_ABSOLUTE, int, GUID*);
    return reinterpret_cast<Function>(original_[3])(actions_, folder, entry, created);
}

HRESULT NativeFrame::AddAtOriginal(PCIDLIST_ABSOLUTE folder, int entry, int index, GUID* created)
{
    using Function = HRESULT (STDMETHODCALLTYPE*)(void*, PCIDLIST_ABSOLUTE, int, int, GUID*);
    return reinterpret_cast<Function>(original_[9])(actions_, folder, entry, index, created);
}

HRESULT NativeFrame::Add(PCIDLIST_ABSOLUTE folder, GUID& created)
{
    created = {};
    std::vector<GUID> before;
    GUID active{};
    if (!folder || !ReadTabs(before, active) || before.size() >= kMaxNativeTabs) return E_FAIL;
    if (available_ && !available_()) return E_ABORT;
    // Entry point 4 is the inspected native tab-drop path. Native acknowledgment
    // precedes any source removal; no explorer.exe launch or whole-frame close.
    const auto result = AddAtOriginal(folder, 4, static_cast<int>(before.size()), &created);
    if (FAILED(result)) return result;
    std::vector<GUID> after;
    if (!ReadTabs(after, active) || std::none_of(after.begin(), after.end(),
        [&created](const GUID& id) { return InlineIsEqualGUID(id, created); })) return E_FAIL;
    if (available_ && !available_()) return E_ABORT;
    using Select = HRESULT (STDMETHODCALLTYPE*)(void*, GUID);
    return reinterpret_cast<Select>(original_[5])(actions_, created);
}

HRESULT NativeFrame::Remove(const GUID& tab, PCIDLIST_ABSOLUTE expected_folder)
{
    std::vector<GUID> tabs;
    GUID active{};
    FolderLocation folder;
    if (!expected_folder || !Snapshot(tabs, active, folder) || !InlineIsEqualGUID(tab, active) ||
        !ILIsEqual(folder.pidl.get(), expected_folder) || (available_ && !available_())) return E_ABORT;
    using Function = HRESULT (STDMETHODCALLTYPE*)(void*, GUID);
    const auto result = reinterpret_cast<Function>(original_[4])(actions_, tab);
    if (FAILED(result) || !IsWindow(root_)) return result;
    if (!ReadTabs(tabs, active) || std::any_of(tabs.begin(), tabs.end(),
        [&tab](const GUID& id) { return InlineIsEqualGUID(id, tab); })) return E_ABORT;
    return S_OK;
}

bool NativeFrame::InstallPlus(std::weak_ptr<void> owner, std::function<bool()> available)
{
    if (!Valid() || intercepting_) return false;
    std::copy_n(original_, proxy_.size(), proxy_.begin());
    proxy_[3] = reinterpret_cast<void*>(&Plus);
    proxy_[9] = reinterpret_cast<void*>(&PlusAt);
    owner_ = std::move(owner);
    available_ = std::move(available);
    proxies[actions_] = shared_from_this();
    *reinterpret_cast<void***>(actions_) = proxy_.data();
    intercepting_ = true;
    return true;
}

void NativeFrame::StopPlus()
{
    if (!intercepting_) return;
    if (*reinterpret_cast<void***>(actions_) == proxy_.data()) *reinterpret_cast<void***>(actions_) = original_;
    proxies.erase(actions_);
    intercepting_ = false;
}

HRESULT STDMETHODCALLTYPE NativeFrame::Plus(void* self, PCIDLIST_ABSOLUTE folder, int entry, GUID* created)
{
    auto frame = FindProxy(self);
    if (!frame) return E_UNEXPECTED;
    const auto lifetime = frame->owner_.lock();
    std::optional<FolderLocation> current;
    try {
        if (!folder && entry == 1 && lifetime && frame->available_()) {
            std::vector<GUID> tabs; GUID active{}; FolderLocation location;
            if (frame->Snapshot(tabs, active, location)) current = std::move(location);
        }
        if (current && frame->Valid() && frame->available_()) folder = current->pidl.get();
    } catch (...) { /* Keep the original native default if location lookup fails. */ }
    if (!frame->Valid()) return E_ABORT;
    try { return frame->AddOriginal(folder, entry, created); }
    catch (...) { return E_UNEXPECTED; } // Never repeat a possibly completed add.
}

HRESULT STDMETHODCALLTYPE NativeFrame::PlusAt(void* self, PCIDLIST_ABSOLUTE folder, int entry, int index, GUID* created)
{
    auto frame = FindProxy(self);
    if (!frame) return E_UNEXPECTED;
    const auto lifetime = frame->owner_.lock();
    std::optional<FolderLocation> current;
    try {
        if (!folder && entry == 1 && lifetime && frame->available_()) {
            std::vector<GUID> tabs; GUID active{}; FolderLocation location;
            if (frame->Snapshot(tabs, active, location)) current = std::move(location);
        }
        if (current && frame->Valid() && frame->available_()) folder = current->pidl.get();
    } catch (...) { /* Keep the original native default if location lookup fails. */ }
    if (!frame->Valid()) return E_ABORT;
    try { return frame->AddAtOriginal(folder, entry, index, created); }
    catch (...) { return E_UNEXPECTED; }
}

} // namespace quickky

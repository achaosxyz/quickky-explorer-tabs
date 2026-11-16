#include "explorer/folder_selection.h"
#include "explorer/native_frame.h"
#include "hook/native_input.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <cwchar>
#include <exdisp.h>
#include <iostream>
#include <servprov.h>
#include <shlguid.h>
#include <stdexcept>
#include <string>
#include <wrl.h>

namespace quickky {
// Typed test doubles only: production binding/ABI recognition is not bypassed.
struct NativeFrameTestAccess {
    static std::shared_ptr<NativeFrame> Create(HWND root, void* object, void* actions,
        void** table, NativeAbiLayout layout, std::function<bool()> available)
    {
        auto frame = std::shared_ptr<NativeFrame>(new NativeFrame);
        frame->root_ = root; frame->thread_ = GetCurrentThreadId();
        frame->object_ = static_cast<unsigned char*>(object); frame->actions_ = actions;
        frame->original_ = table; frame->layout_ = layout; frame->available_ = std::move(available);
        SetWindowLongPtrW(root, 0, reinterpret_cast<LONG_PTR>(object));
        return frame;
    }
};
}

namespace {
using namespace quickky;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;
using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ChainInterfaces;

void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

OwnedPidl Child(unsigned id)
{
    const USHORT bytes[]{4, static_cast<USHORT>(id), 0};
    return CloneFolderSnapshot(bytes, sizeof(bytes));
}

unsigned Id(PCUIDLIST_RELATIVE child)
{
    USHORT id = 0;
    std::memcpy(&id, reinterpret_cast<const unsigned char*>(child) + sizeof(USHORT), sizeof(id));
    return id;
}

FolderSelection Selection(std::initializer_list<unsigned> ids, unsigned focused = 0)
{
    FolderSelection state;
    for (auto id : ids) state.selected.push_back(Child(id));
    if (focused) state.focused = Child(focused);
    return state;
}

// These in-process COM fixtures never enumerate or mutate a real Shell window.
class Items final : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IEnumIDList> {
public:
    std::vector<unsigned> ids;
    std::size_t offset = 0;
    HRESULT STDMETHODCALLTYPE Next(ULONG count, PITEMID_CHILD* children, ULONG* fetched) override
    {
        ULONG found = 0;
        while (found < count && offset < ids.size()) children[found++] = Child(ids[offset++]).release();
        if (fetched) *fetched = found;
        return count == found ? S_OK : S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE Skip(ULONG) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Reset() override { offset = 0; return S_OK; }
    HRESULT STDMETHODCALLTYPE Clone(IEnumIDList**) override { return E_NOTIMPL; }
};

// v141 diagnoses required deprecated overrides when completing the class.
#pragma warning(push)
#pragma warning(disable: 4996)
class View final : public RuntimeClass<RuntimeClassFlags<ClassicCom>,
    ChainInterfaces<IFolderView2, IFolderView>, ChainInterfaces<IPersistFolder2, IPersistFolder, IPersist>,
    ChainInterfaces<IShellView, IOleWindow>> {
public:
    unsigned folder = 70;
    std::vector<unsigned> selected;
    unsigned focused = 0;
    bool ready = true;
    bool ignore_apply = false;
    bool fail_apply = false;
    bool fail_focus = false;
    unsigned writes = 0;
    DWORD focus_flags = 0;
    std::function<void()> after_write;
    HWND window = nullptr;
    std::function<void()> after_get_window;
    HRESULT STDMETHODCALLTYPE GetFolder(REFIID iid, void** out) override { return QueryInterface(iid, out); }
    HRESULT STDMETHODCALLTYPE GetClassID(CLSID*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Initialize(PCIDLIST_ABSOLUTE) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetCurFolder(PIDLIST_ABSOLUTE* out) override { *out = Child(folder).release(); return S_OK; }
    HRESULT STDMETHODCALLTYPE ItemCount(UINT, int* count) override { *count = static_cast<int>(selected.size()); return S_OK; }
    HRESULT STDMETHODCALLTYPE Items(UINT, REFIID iid, void** out) override
    {
        auto items = Make<::Items>();
        items->ids = selected;
        return items.CopyTo(iid, out);
    }
    HRESULT STDMETHODCALLTYPE GetFocusedItem(int* index) override
    { *index = focused ? static_cast<int>(focused) : -1; return fail_focus ? E_FAIL : S_OK; }
    HRESULT STDMETHODCALLTYPE Item(int index, PITEMID_CHILD* out) override { *out = Child(index).release(); return S_OK; }
    HRESULT STDMETHODCALLTYPE GetSelectionState(PCUITEMID_CHILD, DWORD* flags) override
    { *flags = 0; return ready ? S_OK : E_INVALIDARG; }
    HRESULT STDMETHODCALLTYPE SelectAndPositionItems(UINT count, PCUITEMID_CHILD_ARRAY children, POINT*, DWORD flags) override
    {
        ++writes;
        if (fail_apply) return E_FAIL;
        if (!ignore_apply) for (UINT i = 0; i < count; ++i) {
            const auto id = Id(children[i]);
            const auto existing = std::find(selected.begin(), selected.end(), id);
            if (flags & SVSI_SELECT) { if (existing == selected.end()) selected.push_back(id); }
            else if (existing != selected.end()) selected.erase(existing);
            if (flags & SVSI_FOCUSED) { focused = id; focus_flags = flags; }
        }
        if (after_write) after_write();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetCurrentViewMode(UINT*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetCurrentViewMode(UINT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetSelectionMarkedItem(int*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetItemPosition(PCUITEMID_CHILD, POINT*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetSpacing(POINT*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetDefaultSpacing(POINT*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetAutoArrange() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SelectItem(int, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetGroupBy(REFPROPERTYKEY, BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetGroupBy(PROPERTYKEY*, BOOL*) override { return E_NOTIMPL; }
    // Implement required deprecated slots without invoking those APIs.
    HRESULT STDMETHODCALLTYPE SetViewProperty(PCUITEMID_CHILD, REFPROPERTYKEY, REFPROPVARIANT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetViewProperty(PCUITEMID_CHILD, REFPROPERTYKEY, PROPVARIANT*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetTileViewProperties(PCUITEMID_CHILD, LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetExtendedTileViewProperties(PCUITEMID_CHILD, LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetText(FVTEXTTYPE, LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetCurrentFolderFlags(DWORD, DWORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetCurrentFolderFlags(DWORD*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetSortColumnCount(int*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetSortColumns(const SORTCOLUMN*, int) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetSortColumns(SORTCOLUMN*, int) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetItem(int, REFIID, void**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetVisibleItem(int, BOOL, int*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetSelectedItem(int, int*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetSelection(BOOL, IShellItemArray**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE InvokeVerbOnSelection(LPCSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetViewModeAndIconSize(FOLDERVIEWMODE, int) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetViewModeAndIconSize(FOLDERVIEWMODE*, int*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetGroupSubsetCount(UINT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetGroupSubsetCount(UINT*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetRedraw(BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE IsMoveInSameFolder() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DoRename() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetWindow(HWND* out) override
    { *out = window; if (after_get_window) after_get_window(); return S_OK; }
    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE TranslateAccelerator(MSG*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnableModeless(BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE UIActivate(UINT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE Refresh() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE CreateViewWindow(IShellView*, LPCFOLDERSETTINGS, IShellBrowser*, RECT*, HWND*) override
    { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE DestroyViewWindow() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetCurrentInfo(LPFOLDERSETTINGS) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE AddPropertySheetPages(DWORD, LPFNSVADDPROPSHEETPAGE, LPARAM) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SaveViewState() override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SelectItem(PCUITEMID_CHILD, SVSIF) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetItemObject(UINT, REFIID, void**) override { return E_NOTIMPL; }
};
#pragma warning(pop)

// The Shell browser service is distinct from the opaque native tab browser.
class ShellBrowser final : public RuntimeClass<RuntimeClassFlags<ClassicCom>, ChainInterfaces<IShellBrowser, IOleWindow>> {
public:
    ComPtr<View> current;
    std::function<void()> after_get;
    HRESULT STDMETHODCALLTYPE QueryActiveShellView(IShellView** out) override
    {
        const auto result = current.CopyTo(out);
        if (after_get) after_get();
        return result;
    }
    HRESULT STDMETHODCALLTYPE GetWindow(HWND* window) override { *window = nullptr; return S_OK; }
    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE InsertMenusSB(HMENU, LPOLEMENUGROUPWIDTHS) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetMenuSB(HMENU, HOLEMENU, HWND) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE RemoveMenusSB(HMENU) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetStatusTextSB(LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE EnableModelessSB(BOOL) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE TranslateAcceleratorSB(MSG*, WORD) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE BrowseObject(PCUIDLIST_RELATIVE, UINT) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetViewStateStream(DWORD, IStream**) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE GetControlWindow(UINT, HWND*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SendControlMsg(UINT, UINT, WPARAM, LPARAM, LRESULT*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE OnViewWindowActive(IShellView*) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE SetToolbarItems(LPTBBUTTONSB, UINT, UINT) override { return E_NOTIMPL; }
};

// Native tab browsers expose the service, not direct browser/control interfaces.
class Browser final : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IServiceProvider> {
public:
    ComPtr<ShellBrowser> shell = Make<ShellBrowser>();
    HRESULT STDMETHODCALLTYPE QueryService(REFGUID service, REFIID iid, void** out) override
    {
        if (!out) return E_POINTER;
        *out = nullptr;
        if (!IsEqualGUID(service, SID_STopLevelBrowser)) return E_NOINTERFACE;
        return shell.CopyTo(iid, out);
    }
};

struct NativeTab { void* browser; void* site; GUID id; std::uint64_t flags; };
struct Actions {
    void** table = nullptr;
    NativeTab* first = nullptr;
    NativeTab* last = nullptr;
    void* active = nullptr;
    View* view = nullptr;
    unsigned removals = 0;
    HRESULT remove_result = S_OK;
};
ULONG STDMETHODCALLTYPE ReleaseActions(void*) { return 1; }
HRESULT STDMETHODCALLTYPE ReadTab(void* self, void* data)
{
    struct Data { GUID id; PWSTR title; PIDLIST_ABSOLUTE folder; };
    static_cast<Data*>(data)->folder = Child(static_cast<Actions*>(self)->view->folder).release();
    return S_OK;
}
HRESULT STDMETHODCALLTYPE RemoveTab(void* self, GUID)
{
    auto& actions = *static_cast<Actions*>(self);
    ++actions.removals;
    if (FAILED(actions.remove_result)) return actions.remove_result;
    actions.last = actions.first;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE SelectTab(void* self, GUID id)
{
    auto& actions = *static_cast<Actions*>(self);
    if (!InlineIsEqualGUID(actions.first->id, id)) return E_INVALIDARG;
    actions.active = actions.first->browser;
    return S_OK;
}

struct FrameCase {
    int object = 0;
    HWND root = nullptr;
    void* table[13]{};
    ComPtr<View> view = Make<View>();
    ComPtr<Browser> browser = Make<Browser>();
    NativeTab tab{};
    Actions actions;
    bool allowed = true;
    std::shared_ptr<NativeFrame> frame;
    FrameCase()
    {
        WNDCLASSW type{};
        type.lpfnWndProc = DefWindowProcW; type.cbWndExtra = sizeof(void*);
        type.hInstance = GetModuleHandleW(nullptr); type.lpszClassName = L"QTabs.SelectionFrameFixture";
        RegisterClassW(&type);
        root = CreateWindowW(type.lpszClassName, L"Selection fixture", WS_OVERLAPPEDWINDOW,
            -30000, -30000, 320, 240, nullptr, nullptr, type.hInstance, nullptr);
        Check(root != nullptr, "Create owned native-frame fixture");
        browser->shell->current = view;
        tab = {static_cast<IServiceProvider*>(browser.Get()), nullptr, {1}, 0};
        table[2] = reinterpret_cast<void*>(&ReleaseActions);
        table[4] = reinterpret_cast<void*>(&RemoveTab); table[8] = reinterpret_cast<void*>(&ReadTab);
        table[5] = reinterpret_cast<void*>(&SelectTab);
        actions = {table, &tab, &tab + 1, tab.browser, view.Get(), 0};
        NativeAbiLayout layout;
        layout.tabs = offsetof(Actions, first); layout.active = offsetof(Actions, active);
        frame = NativeFrameTestAccess::Create(root, &object, &actions, table, layout, [&] { return allowed; });
    }
    ~FrameCase() { frame.reset(); if (root) DestroyWindow(root); }
};

void TestFrameSelectionGuards()
{
    FrameCase fixture;
    fixture.view->selected = {2, 3}; fixture.view->focused = 3;
    std::vector<GUID> tabs;
    GUID active{};
    FolderLocation folder;
    FolderSelection captured;
    ComPtr<IExplorerBrowser> embedded;
    Check(fixture.browser.As(&embedded) == E_NOINTERFACE,
        "Native-host fixture must reject the embedded IExplorerBrowser interface");
    ComPtr<IShellBrowser> direct;
    Check(fixture.browser.As(&direct) == E_NOINTERFACE,
        "Native-host fixture must require the Shell browser service, not a direct interface");
    Check(fixture.frame->CaptureSelection(tabs, active, folder, captured) == S_OK &&
        SameSelection(captured, Selection({2, 3}, 3)), "Capture via active browser public view");
    bool applied = false;
    fixture.view->selected = {};
    bool folder_ready = true;
    Check(fixture.frame->RestoreSelection(GUID{99}, folder.pidl.get(), captured, applied, folder_ready, [] { return true; }) == E_ABORT &&
        !fixture.view->writes, "A different active GUID must never receive a selection write");
    Check(fixture.frame->RestoreSelection(active, folder.pidl.get(), captured, applied, folder_ready, [] { return false; }) == E_ABORT &&
        !fixture.view->writes, "Target input change cancels before restoring");
    fixture.allowed = false;
    Check(fixture.frame->RestoreSelection(active, folder.pidl.get(), captured, applied, folder_ready, [] { return true; }) == E_ABORT,
        "Canceled lease prevents restoring");
    fixture.allowed = true;
    Check(fixture.frame->Remove(active, folder.pidl.get(), captured, [] { return true; }) == E_ABORT && !fixture.actions.removals,
        "Changed source selection prevents source removal");
    Check(fixture.frame->RestoreSelection(active, folder.pidl.get(), captured, applied, folder_ready, [] { return true; }) == S_OK,
        "Restore through the native-frame owning-thread boundary");
    fixture.view->focused = 2;
    Check(fixture.frame->Remove(active, folder.pidl.get(), captured, [] { return true; }) == E_ABORT && !fixture.actions.removals,
        "Changed source focus also prevents removal");
    fixture.view->focused = 3;
    Check(fixture.frame->Remove(active, folder.pidl.get(), captured, [] { return false; }) == E_ABORT && !fixture.actions.removals,
        "Changed source input epoch prevents deletion");
    Check(fixture.frame->Remove(active, folder.pidl.get(), captured, [] { return true; }) == S_OK && fixture.actions.removals == 1,
        "Unchanged selection permits exactly one source removal");

    FrameCase replaced;
    auto expected = Selection({2}, 2);
    auto location = Child(70);
    replaced.view->selected = {9};
    auto replacement = Make<View>();
    replaced.view->after_write = [&] { replaced.browser->shell->current = replacement; };
    applied = false;
    folder_ready = false;
    Check(replaced.frame->RestoreSelection(replaced.tab.id, location.get(), expected, applied, folder_ready, [] { return true; }) == E_ABORT &&
        replaced.view->writes == 1 && !replacement->writes, "Reentrant view replacement cannot receive subsequent writes");
    FrameCase switched;
    switched.browser->shell->after_get = [&] { switched.tab.id = GUID{2}; };
    Check(switched.frame->CaptureSelection(tabs, active, folder, captured) == E_ABORT,
        "A source tab change during a public view call invalidates capture");
}

void TestInitialFolderReadiness()
{
    FrameCase loading;
    loading.view->folder = 71;
    const auto folder = Child(70);
    auto expected = Selection({2}, 2);
    bool applied = false;
    bool folder_ready = false;
    Check(loading.frame->RestoreSelection(loading.tab.id, folder.get(), expected, applied, folder_ready,
        [] { return true; }) == E_PENDING && !applied && !loading.view->writes,
        "A created tab must wait for its initial folder, not abandon the source");
    loading.view->folder = 70;
    loading.view->ready = false;
    Check(loading.frame->RestoreSelection(loading.tab.id, folder.get(), expected, applied, folder_ready,
        [] { return true; }) == E_PENDING && folder_ready && !applied,
        "Initial folder observation must survive later readiness polls");
    loading.view->ready = true;
    Check(loading.frame->RestoreSelection(loading.tab.id, folder.get(), expected, applied, folder_ready,
        [] { return true; }) == S_OK && applied && loading.view->selected == std::vector<unsigned>{2},
        "The same created tab must finish when its view becomes ready");
    const auto writes = loading.view->writes;
    Check(loading.frame->RestoreSelection(loading.tab.id, folder.get(), expected, applied, folder_ready,
        [] { return true; }) == S_OK && loading.view->writes == writes,
        "Continuing the same restore must not repeat selection writes");
    FrameCase changed;
    changed.view->ready = false; applied = false; folder_ready = false;
    Check(changed.frame->RestoreSelection(changed.tab.id, folder.get(), expected, applied, folder_ready,
        [] { return true; }) == E_PENDING && folder_ready, "Observe initial folder before delayed children");
    changed.view->folder = 71;
    Check(changed.frame->RestoreSelection(changed.tab.id, folder.get(), expected, applied, folder_ready,
        [] { return true; }) == E_ABORT && !changed.view->writes,
        "A later folder change must not be mistaken for initial loading");
    Check(IsNativeRestoreProgress(0) && IsNativeRestoreProgress(kNativeRestoreFolderReady) &&
        IsNativeRestoreProgress(kNativeRestoreFolderReady | kNativeRestoreWriteConsumed) &&
        !IsNativeRestoreProgress(kNativeRestoreWriteConsumed) && !IsNativeRestoreProgress(4),
        "Only valid restore-progress transitions are accepted");
}

HWND FocusChild(HWND root)
{
    const auto child = CreateWindowW(L"STATIC", L"Owned focus fixture", WS_CHILD | WS_VISIBLE,
        0, 0, 100, 60, root, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(child != nullptr, "Create a test-owned view/control window");
    return child;
}

LRESULT CALLBACK CancelOnFocus(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_SETFOCUS) {
        const auto allowed = reinterpret_cast<bool*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (allowed) *allowed = false;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

void TestNativeViewFocus()
{
    for (const bool maximized : {false, true}) for (const bool minimized : {false, true})
        for (const bool selected : {false, true}) {
            FrameCase fixture;
            fixture.view->window = FocusChild(fixture.root);
            const auto other = FocusChild(fixture.root);
            if (selected) { fixture.view->selected = {2, 3}; fixture.view->focused = 3; }
            ShowWindow(fixture.root, maximized ? SW_SHOWMAXIMIZED : SW_SHOWNOACTIVATE);
            WINDOWPLACEMENT before{sizeof(before)};
            Check(GetWindowPlacement(fixture.root, &before) != FALSE, "Read pre-activation placement");
            SetFocus(other);
            Check(GetFocus() == other, "Start keyboard focus outside the file view");
            if (minimized) ShowWindow(fixture.root, SW_MINIMIZE);
            const auto result = fixture.frame->Reveal(fixture.tab.id, [] { return true; });
            Check((result == S_OK || result == E_ACCESSDENIED) && !IsIconic(fixture.root) &&
                (IsZoomed(fixture.root) != FALSE) == maximized,
                "Every destination state must request activation without changing placement");
            if (result == S_OK) Check(GetForegroundWindow() == fixture.root &&
                GetFocus() == fixture.view->window,
                "Successful activation must focus the file view even with no selected files");
            Check(!fixture.view->writes && fixture.view->selected ==
                (selected ? std::vector<unsigned>{2, 3} : std::vector<unsigned>{}) &&
                fixture.view->focused == (selected ? 3u : 0u),
                "Keyboard activation is separate from focused-item/selection restoration");
            WINDOWPLACEMENT after{sizeof(after)};
            Check(GetWindowPlacement(fixture.root, &after) &&
                EqualRect(&before.rcNormalPosition, &after.rcNormalPosition),
                "Normal and maximized destinations retain their original placement");
        }

    FrameCase fixture;
    fixture.view->window = FocusChild(fixture.root);
    const auto other = FocusChild(fixture.root);
    ShowWindow(fixture.root, SW_SHOWNOACTIVATE);
    SetFocus(other);
    bool unchanged = true;
    fixture.view->after_get_window = [&] { unchanged = false; };
    Check(fixture.frame->Reveal(fixture.tab.id, [&] { return unchanged; }) == E_ABORT && GetFocus() == other,
        "Later destination input during COM lookup must prevent file-view focus");
    fixture.view->after_get_window = {};
    auto replacement = Make<View>();
    replacement->window = FocusChild(fixture.root);
    fixture.view->after_get_window = [&] { fixture.browser->shell->current = replacement; };
    Check(fixture.frame->Reveal(fixture.tab.id, [] { return true; }) == E_ABORT && GetFocus() == other,
        "A replaced Shell view must not receive stale keyboard focus");
    fixture.view->after_get_window = {};
    fixture.browser->shell->current = fixture.view;
    const auto recreated_window = FocusChild(fixture.root);
    unsigned view_queries = 0;
    fixture.browser->shell->after_get = [&] {
        if (++view_queries == 2) fixture.view->window = recreated_window;
    };
    Check(fixture.frame->Reveal(fixture.tab.id, [] { return true; }) == E_ABORT && GetFocus() == other,
        "A recreated window on the same COM view must invalidate the captured focus HWND");
    fixture.browser->shell->after_get = {};
    FrameCase foreign;
    fixture.view->window = FocusChild(foreign.root);
    Check(fixture.frame->Reveal(fixture.tab.id, [] { return true; }) == E_ABORT && GetFocus() == other,
        "A public view cannot redirect focus into a different root");

    FrameCase during_focus;
    WNDCLASSW type{};
    type.lpfnWndProc = CancelOnFocus; type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = L"QTabs.FocusCancellationFixture";
    Check(RegisterClassW(&type) != 0, "Register an owned focus-callout fixture");
    during_focus.view->window = CreateWindowW(type.lpszClassName, L"Owned focus callback",
        WS_CHILD | WS_VISIBLE, 0, 0, 100, 60, during_focus.root, nullptr, type.hInstance, nullptr);
    Check(during_focus.view->window != nullptr, "Create an owned focus-callout view");
    const auto initial = FocusChild(during_focus.root);
    ShowWindow(during_focus.root, SW_SHOWNOACTIVATE);
    SetFocus(initial);
    SetWindowLongPtrW(during_focus.view->window, GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(&during_focus.allowed));
    const auto focused = during_focus.frame->Reveal(during_focus.tab.id, [] { return true; });
    Check((focused == E_ABORT && !during_focus.allowed) ||
        (focused == E_ACCESSDENIED && during_focus.allowed),
        "Focus cancellation or foreground denial must not report successful activation");

    FrameCase denied;
    denied.view->window = CreateWindowW(type.lpszClassName, L"Owned foreground-denial view",
        WS_CHILD | WS_VISIBLE, 0, 0, 100, 60, denied.root, nullptr, type.hInstance, nullptr);
    Check(denied.view->window != nullptr, "Create a foreground-denial focus sentinel");
    ShowWindow(denied.root, SW_SHOWNOACTIVATE);
    SetFocus(FocusChild(denied.root));
    SetWindowLongPtrW(denied.view->window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&denied.allowed));
    ShowWindow(foreign.root, SW_SHOWNOACTIVATE);
    SetForegroundWindow(foreign.root);
    SetFocus(fixture.view->window);
    unsigned activation_queries = 0;
    bool foreground_seen = false, switch_attempted = false, switched = false;
    denied.browser->shell->after_get = [&] {
        foreground_seen = foreground_seen || GetForegroundWindow() == denied.root;
        if (++activation_queries == 3) {
            switch_attempted = true;
            SetForegroundWindow(foreign.root);
            switched = GetForegroundWindow() == foreign.root;
        }
    };
    const auto refused = denied.frame->Reveal(denied.tab.id, [] { return true; });
    Check(!foreground_seen || switch_attempted,
        "Focus must not run before the post-activation view/foreground check");
    if (!switch_attempted || switched) Check(refused == E_ACCESSDENIED && denied.allowed &&
        GetFocus() != denied.view->window,
        "Foreground denial must prevent keyboard focus, including after a COM callout");
    else std::cout << "Controlled foreground switch denied by Windows; late-denial assertion skipped.\n";
}

void TestNativeInput()
{
    Check(!IsNativeTransferInput(WM_MOUSEMOVE, 0), "Ordinary hover must not cancel a native transfer");
    Check(!IsNativeTransferInput(WM_MOUSEMOVE, MK_CONTROL | MK_SHIFT), "Modifiers alone do not turn hover into a drag");
    for (const auto button : {MK_LBUTTON, MK_RBUTTON, MK_MBUTTON, MK_XBUTTON1, MK_XBUTTON2})
        Check(IsNativeTransferInput(WM_MOUSEMOVE, button), "Button-held motion must cancel a native transfer");
    for (const auto message : {WM_KEYDOWN, WM_KEYUP, WM_CHAR, WM_LBUTTONDOWN, WM_LBUTTONUP,
        WM_RBUTTONDBLCLK, WM_MOUSEWHEEL, WM_MOUSEHWHEEL, WM_XBUTTONDOWN, WM_NCLBUTTONDOWN, WM_NCXBUTTONUP})
        Check(IsNativeTransferInput(message, 0), "Actionable input must still cancel a native transfer");
    Check(!IsNativeTransferInput(WM_TIMER, 0) && !IsNativeTransferInput(WM_MOUSELEAVE, 0),
        "Timer and hover bookkeeping are not native-transfer input");
}

void TestFinalRemovalGuards()
{
    const auto folder = Child(70);
    auto selected = Selection({2}, 2);
    FrameCase hover;
    hover.view->selected = {2}; hover.view->focused = 2;
    unsigned epoch = 1;
    for (unsigned i = 0; i < 20; ++i) if (IsNativeTransferInput(WM_MOUSEMOVE, 0)) ++epoch;
    Check(hover.frame->Remove(hover.tab.id, folder.get(), selected, [&] { return epoch == 1; }) == S_OK &&
        hover.actions.removals == 1, "Hover must not strand a confirmed source tab");

    FrameCase pending;
    pending.view->selected = {2}; pending.view->focused = 2; pending.view->fail_focus = true;
    Check(pending.frame->Remove(pending.tab.id, folder.get(), selected, [] { return true; }) == E_PENDING &&
        !pending.actions.removals, "Source read readiness must continue before any native remove");
    pending.view->fail_focus = false;
    pending.actions.remove_result = E_PENDING;
    Check(pending.frame->Remove(pending.tab.id, folder.get(), selected, [] { return true; }) == E_UNEXPECTED &&
        pending.actions.removals == 1, "An invoked native remove must never be treated as a retryable readiness poll");

    HANDLE peer = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    Check(peer != nullptr, "Create owned destination-input fence");
    struct CloseEvent { HANDLE value; ~CloseEvent() { CloseHandle(value); } } close{peer};
    FrameCase source;
    source.view->selected = {2}; source.view->focused = 2;
    source.browser->shell->after_get = [&] { SetEvent(peer); };
    Check(source.frame->Remove(source.tab.id, folder.get(), selected,
        [&] { return WaitForSingleObject(peer, 0) == WAIT_TIMEOUT; }) == E_ABORT && !source.actions.removals,
        "Destination input during source COM reads must prevent final native removal");
    source.browser->shell->after_get = {};
    ResetEvent(peer);
    Check(source.frame->Remove(source.tab.id, folder.get(), selected,
        [&] { return WaitForSingleObject(peer, 0) == WAIT_TIMEOUT; }) == S_OK && source.actions.removals == 1,
        "An unchanged destination fence allows exactly one native source removal");
}

void TestEncoding()
{
    std::array<unsigned char, kMaxSelectionBytes> bytes{};
    std::uint32_t length = 0;
    FolderSelection decoded;
    auto state = Selection({2, 3}, 3);
    Check(EncodeSelection(state, bytes.data(), bytes.size(), length) && length > 8, "Encode selected children and focused child");
    Check(DecodeSelection(bytes.data(), length, decoded) && SameSelection(state, decoded), "Selection round trip");
    std::reverse(decoded.selected.begin(), decoded.selected.end());
    Check(SameSelection(state, decoded), "Enumeration order is not selection identity");
    for (std::uint32_t truncated = 0; truncated < length; ++truncated)
        Check(!DecodeSelection(bytes.data(), truncated, decoded), "Reject every truncated payload");
    Check(!DecodeSelection(bytes.data(), length + 1, decoded), "Reject trailing data");
    Check(!DecodeSelection(nullptr, length, decoded), "Reject missing storage");
    Check(!DecodeSelection(bytes.data(), bytes.size() + 1, decoded), "Reject oversized wire length");
    auto empty = Selection({});
    Check(EncodeSelection(empty, bytes.data(), bytes.size(), length) && length == 8 &&
        DecodeSelection(bytes.data(), length, decoded) && SameSelection(empty, decoded), "Empty selection is explicit, not missing");
    auto duplicate = Selection({2, 2});
    Check(EncodeSelection(duplicate, bytes.data(), bytes.size(), length) &&
        !DecodeSelection(bytes.data(), length, decoded), "Reject duplicate selected PIDLs");
    auto invalid = Selection({2});
    invalid.selected[0].reset();
    Check(!EncodeSelection(invalid, bytes.data(), bytes.size(), length), "Reject null selected child");
    for (unsigned i = 1; i <= kMaxSelectionItems + 1; ++i) empty.selected.push_back(Child(i));
    Check(!EncodeSelection(empty, bytes.data(), bytes.size(), length), "Bound selected item count");
    std::uint32_t excessive = kMaxSelectionItems + 1;
    std::memcpy(bytes.data(), &excessive, sizeof(excessive));
    Check(!DecodeSelection(bytes.data(), 8, decoded), "Bound received item count before allocating");
    Check(!EncodeSelection(state, bytes.data(), 4, length), "Reject insufficient output capacity");
    std::vector<unsigned char> large(60002, 0);
    USHORT large_size = 60000;
    std::memcpy(large.data(), &large_size, sizeof(large_size));
    FolderSelection oversized;
    oversized.selected.push_back(CloneFolderSnapshot(large.data(), large.size()));
    oversized.focused = CloneFolderSnapshot(large.data(), large.size());
    Check(!EncodeSelection(oversized, bytes.data(), bytes.size(), length), "Bound cumulative PIDL bytes independently of item count");
}

void TestRestore()
{
    const auto folder = Child(70);
    auto view = Make<View>();
    view->selected = {2, 3}; view->focused = 3;
    FolderSelection captured;
    Check(ReadSelection(view.Get(), folder.get(), captured) == S_OK &&
        SameSelection(captured, Selection({2, 3}, 3)), "Read file selection and focus from view");
    view->fail_focus = true;
    FolderSelection incomplete;
    Check(ReadSelection(view.Get(), folder.get(), incomplete) == E_PENDING && incomplete.selected.empty(),
        "A failed focus read is not a successful no-focus snapshot");
    view->fail_focus = false;
    view->selected = {9}; view->focused = 9;
    bool applied = false;
    bool available = true;
    const auto valid = [&] { return available; };
    view->ready = false;
    Check(RestoreSelection(view.Get(), folder.get(), captured, applied, valid) == E_PENDING && !applied && !view->writes,
        "Wait for enumerated children without changing selection");
    view->ready = true;
    Check(RestoreSelection(view.Get(), folder.get(), captured, applied, valid) == S_OK && applied && view->writes == 3 &&
        view->selected == std::vector<unsigned>({2, 3}) && view->focused == 3,
        "Restore multiple selected files and keep the focused file selected");
    Check((view->focus_flags & SVSI_ENSUREVISIBLE) && (view->focus_flags & SVSI_NOTAKEFOCUS), "Ensure item visibility without stealing window focus");
    const auto writes = view->writes;
    Check(RestoreSelection(view.Get(), folder.get(), captured, applied, valid) == S_OK && view->writes == writes,
        "Successful confirmation does not write twice");
    view->selected = {6};
    Check(RestoreSelection(view.Get(), folder.get(), captured, applied, valid) == E_PENDING && view->writes == writes,
        "Do not overwrite a later selection while waiting for confirmation");
    available = false;
    Check(RestoreSelection(view.Get(), folder.get(), captured, applied, valid) == E_ABORT && view->writes == writes,
        "Cancel after input/lease change without writing");
    available = true;
    applied = false;
    view->folder = 71;
    Check(RestoreSelection(view.Get(), folder.get(), captured, applied, valid) == E_PENDING && !applied,
        "Wait for the expected folder rather than selecting in another view");
    view->folder = 70;
    auto focus_only = Selection({}, 5);
    Check(RestoreSelection(view.Get(), folder.get(), focus_only, applied, valid) == S_OK &&
        view->selected.empty() && view->focused == 5, "Focus can belong to an unselected item");
    applied = false;
    view->selected = {2, 3};
    Check(RestoreSelection(view.Get(), folder.get(), Selection({}), applied, valid) == S_OK && view->selected.empty(),
        "Restore empty selection without requiring nonexistent focus");
}

void TestReentrancyAndDeferredConfirmation()
{
    const auto folder = Child(70);
    auto selection = Selection({2}, 2);
    auto view = Make<View>();
    bool available = true;
    bool applied = false;
    view->selected = {9};
    view->after_write = [&] { available = false; };
    Check(RestoreSelection(view.Get(), folder.get(), selection, applied, [&] { return available; }) == E_ABORT && view->writes == 1,
        "Reentrant cancellation after deselection prevents subsequent writes");
    view = Make<View>(); available = true; applied = false;
    view->selected = {9}; view->after_write = [&] { view->folder = 71; };
    Check(RestoreSelection(view.Get(), folder.get(), selection, applied, [] { return true; }) == E_ABORT && view->writes == 1,
        "Reentrant navigation prevents selecting in the replacement folder");
    view = Make<View>(); applied = false; view->ignore_apply = true;
    Check(RestoreSelection(view.Get(), folder.get(), selection, applied, [] { return true; }) == E_PENDING && applied,
        "A write acknowledgment is not selection confirmation");
    const auto writes = view->writes;
    Check(RestoreSelection(view.Get(), folder.get(), selection, applied, [] { return true; }) == E_PENDING && view->writes == writes,
        "Repeated readiness checks never repeat an accepted write");
    view->selected = {2}; view->focused = 2;
    Check(RestoreSelection(view.Get(), folder.get(), selection, applied, [] { return true; }) == S_OK && view->writes == writes,
        "Confirm deferred selection on a later UI tick");
    view = Make<View>(); applied = false; view->fail_apply = true;
    Check(RestoreSelection(view.Get(), folder.get(), selection, applied, [] { return true; }) == E_FAIL && applied && view->writes == 1,
        "Failed selection is not safe to repeat");
}

void TestStability()
{
    SelectionStability empty;
    Check(!empty.Observe(0, false, true) && !empty.Observe(199, true, true) && empty.Observe(200, true, true),
        "Initially empty views receive bounded grace for late /select");
    SelectionStability selected;
    Check(!selected.Observe(1000, false, false) && !selected.Observe(1049, true, false) && selected.Observe(1050, true, false),
        "Nonempty selections require two stable observations");
    SelectionStability delayed;
    Check(!delayed.Observe(0, false, true) && !delayed.Observe(180, false, false) &&
        !delayed.Observe(229, true, false) && delayed.Observe(230, true, false), "A late selection resets the stability interval");
}

// Optional local verification uses real public Shell controls owned by this test
// process. It does not inject into, enumerate, or close daily Explorer windows.
struct TestDirectory {
    std::wstring path;
    TestDirectory()
    {
        wchar_t temporary[MAX_PATH]{}, name[40]{};
        GUID id{};
        Check(GetTempPathW(MAX_PATH, temporary) && SUCCEEDED(CoCreateGuid(&id)) &&
            StringFromGUID2(id, name, 40), "Create isolated Shell fixture identity");
        const std::wstring candidate = std::wstring(temporary) + L"QTabs-selection-" + name;
        Check(CreateDirectoryW(candidate.c_str(), nullptr) != FALSE, "Create test-owned directory");
        path = candidate;
        for (const auto file : {L"selected.txt", L"second.txt"}) {
            const auto location = path + L"\\" + file;
            HANDLE handle = CreateFileW(location.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            Check(handle != INVALID_HANDLE_VALUE, "Create test-owned selected file");
            CloseHandle(handle);
        }
    }
    ~TestDirectory()
    {
        if (!path.empty()) {
            DeleteFileW((path + L"\\selected.txt").c_str());
            DeleteFileW((path + L"\\second.txt").c_str());
            RemoveDirectoryW((path + L"\\Empty").c_str());
            RemoveDirectoryW(path.c_str());
        }
    }
};

struct ShellView {
    HWND root = nullptr;
    ComPtr<IExplorerBrowser> browser;
    ComPtr<IFolderView> view;
    ~ShellView()
    {
        view.Reset();
        if (browser) browser->Destroy();
        browser.Reset();
        if (root) DestroyWindow(root);
    }
    void Create(PCIDLIST_ABSOLUTE folder)
    {
        root = CreateWindowW(L"STATIC", L"QTabs owned Shell selection fixture", WS_OVERLAPPEDWINDOW,
            -30000, -30000, 640, 480, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        Check(root != nullptr, "Create hidden test-owned host");
        Check(SUCCEEDED(CoCreateInstance(CLSID_ExplorerBrowser, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&browser))), "Create public ExplorerBrowser control");
        RECT rect{0, 0, 640, 480};
        FOLDERSETTINGS settings{FVM_DETAILS, FWF_AUTOARRANGE};
        Check(SUCCEEDED(browser->Initialize(root, &rect, &settings)) &&
            SUCCEEDED(browser->BrowseToIDList(folder, SBSP_ABSOLUTE)), "Navigate only the fixture folder");
    }
};

template<typename Predicate> bool PumpUntil(Predicate predicate)
{
    const auto deadline = GetTickCount64() + 5000;
    do {
        if (predicate()) return true;
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message); DispatchMessageW(&message);
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    } while (GetTickCount64() < deadline);
    return false;
}

void TestOwnedShellViews()
{
    TestDirectory directory;
    PIDLIST_ABSOLUTE parsed = nullptr;
    Check(SUCCEEDED(SHParseDisplayName(directory.path.c_str(), nullptr, &parsed, 0, nullptr)), "Parse owned directory");
    OwnedPidl folder(parsed);
    FolderSelection intended;
    for (const auto name : {L"selected.txt", L"second.txt"}) {
        parsed = nullptr;
        Check(SUCCEEDED(SHParseDisplayName((directory.path + L"\\" + name).c_str(), nullptr, &parsed, 0, nullptr)), "Parse owned file");
        OwnedPidl absolute(parsed);
        intended.selected.emplace_back(ILCloneFull(ILFindLastID(absolute.get())));
    }
    intended.focused.reset(ILCloneFull(intended.selected[1].get()));
    ShellView source, destination;
    source.Create(folder.get()); destination.Create(folder.get());
    const auto ready = [](ShellView& shell) {
        if (FAILED(shell.browser->GetCurrentView(IID_PPV_ARGS(&shell.view)))) return false;
        int count = 0;
        return SUCCEEDED(shell.view->ItemCount(SVGIO_ALLVIEW, &count)) && count == 2;
    };
    Check(PumpUntil([&] { return ready(source) && ready(destination); }), "Wait for real Shell enumeration");
    bool applied = false;
    HRESULT result = E_PENDING;
    Check(PumpUntil([&] {
        result = RestoreSelection(source.view.Get(), folder.get(), intended, applied, [] { return true; });
        return result != E_PENDING;
    }) && result == S_OK, "Select/focus real source files");
    FolderSelection captured;
    Check(ReadSelection(source.view.Get(), folder.get(), captured) == S_OK && SameSelection(intended, captured), "Read actual Shell file selection");
    applied = false;
    Check(PumpUntil([&] {
        result = RestoreSelection(destination.view.Get(), folder.get(), captured, applied, [] { return true; });
        return result != E_PENDING;
    }) && result == S_OK, "Restore real destination file selection and focus");
    FolderSelection confirmed;
    Check(ReadSelection(destination.view.Get(), folder.get(), confirmed) == S_OK && SameSelection(captured, confirmed),
        "Confirm exact real Shell selection and focused item");
    Check(CreateDirectoryW((directory.path + L"\\Empty").c_str(), nullptr) != FALSE, "Create owned empty folder");
    parsed = nullptr;
    Check(SUCCEEDED(SHParseDisplayName((directory.path + L"\\Empty").c_str(), nullptr, &parsed, 0, nullptr)),
        "Parse owned empty folder");
    OwnedPidl empty_folder(parsed);
    ShellView empty;
    empty.Create(empty_folder.get());
    FolderSelection no_items;
    Check(PumpUntil([&] {
        return SUCCEEDED(empty.browser->GetCurrentView(IID_PPV_ARGS(&empty.view))) &&
            ReadSelection(empty.view.Get(), empty_folder.get(), no_items) == S_OK;
    }), "Capture empty public view with absent focus");
    Check(no_items.selected.empty() && !no_items.focused, "Empty public view reports no selection or focus");
    std::cout << "Owned public Shell view transfer passed\n";
}
// Explicit read-only local diagnostic. No hook, navigation, selection write,
// activation or window-close call is made on an enumerated native Explorer host.
void TestNativeViewsReadOnly()
{
    ComPtr<IShellWindows> windows;
    Check(SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&windows))),
        "Read-only native Shell enumeration");
    long count = 0;
    Check(SUCCEEDED(windows->get_Count(&count)), "Read native Shell view count");
    unsigned checked = 0;
    for (long i = 0; i < count && checked < 32; ++i) {
        VARIANT index{};
        index.vt = VT_I4; index.lVal = i;
        ComPtr<IDispatch> dispatch;
        ComPtr<IWebBrowserApp> window;
        SHANDLE_PTR handle = 0;
        if (FAILED(windows->Item(index, &dispatch)) || !dispatch || FAILED(dispatch.As(&window)) ||
            FAILED(window->get_HWND(&handle))) continue;
        wchar_t kind[64]{};
        if (!GetClassNameW(reinterpret_cast<HWND>(handle), kind, 64) || std::wcscmp(kind, L"CabinetWClass")) continue;
        ComPtr<IServiceProvider> services;
        ComPtr<IShellBrowser> host;
        Check(SUCCEEDED(window.As(&services)) &&
            SUCCEEDED(services->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&host))), "Resolve native Shell host");
        ComPtr<IFolderView> view;
        const auto resolved = ResolveNativeFolderView(window.Get(), &view);
        Check(resolved == S_OK, "Production native-view resolver failed on this machine");
        ComPtr<IPersistFolder2> persist;
        Check(SUCCEEDED(view->GetFolder(IID_PPV_ARGS(&persist))), "Read native view folder");
        PIDLIST_ABSOLUTE raw = nullptr;
        Check(SUCCEEDED(persist->GetCurFolder(&raw)) && raw, "Read native folder identity");
        OwnedPidl folder(raw);
        if (!DescribeFolder(OwnedPidl(ILCloneFull(folder.get())))) continue;
        FolderSelection selection;
        const auto captured = ReadSelection(view.Get(), folder.get(), selection);
        ComPtr<IExplorerBrowser> embedded;
        const auto control = host.As(&embedded);
        std::cout << "native-view=" << checked << " embedded-interface=0x" << std::hex <<
            static_cast<unsigned long>(control) << " selection=0x" << static_cast<unsigned long>(captured) <<
            std::dec << " selected=" << selection.selected.size() << "\n";
        Check(captured == S_OK, "Native view selection capture failed on this machine");
        ++checked;
    }
    Check(checked != 0, "No eligible native Explorer views for read-only diagnostic");
    std::cout << "Read-only native Explorer views passed: " << checked << "\n";
}
} // namespace

int main(int argc, char** argv)
{
    const auto initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        Check(SUCCEEDED(initialized), "Initialize test-only COM apartment");
        TestEncoding(); TestRestore(); TestReentrancyAndDeferredConfirmation(); TestStability(); TestFrameSelectionGuards();
        TestInitialFolderReadiness(); TestNativeViewFocus(); TestNativeInput(); TestFinalRemovalGuards();
        if (argc == 2 && std::strcmp(argv[1], "--shell-view") == 0) TestOwnedShellViews();
        else if (argc == 2 && std::strcmp(argv[1], "--native-views") == 0) TestNativeViewsReadOnly();
        CoUninitialize();
        std::cout << "Folder selection tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        if (SUCCEEDED(initialized)) CoUninitialize();
        std::cerr << error.what() << '\n';
        return 1;
    }
}

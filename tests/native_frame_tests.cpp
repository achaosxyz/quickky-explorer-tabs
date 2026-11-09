#include "explorer/native_frame.h"

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace quickky {
// Bypass ABI discovery only for a synthetic root and typed fake actions. This
// fixture exercises presentation/lifetime checks, not real Explorer transfer.
struct NativeFrameTestAccess {
    static void Available(NativeFrame& frame, std::function<bool()> predicate)
    { frame.available_ = std::move(predicate); }
    static std::shared_ptr<NativeFrame> Create(HWND root, void* object, void* actions,
        void** methods, NativeAbiLayout layout, std::function<bool()> available)
    {
        auto frame = std::shared_ptr<NativeFrame>(new NativeFrame);
        frame->root_ = root;
        frame->thread_ = GetCurrentThreadId();
        frame->object_ = static_cast<unsigned char*>(object);
        frame->actions_ = actions;
        frame->original_ = methods;
        frame->layout_ = layout;
        frame->available_ = std::move(available);
        SetWindowLongPtrW(root, 0, reinterpret_cast<LONG_PTR>(object));
        return frame;
    }
};
}

namespace {
void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct Tab {
    void* browser;
    void* site;
    GUID id;
    std::uint64_t flags;
};
static_assert(sizeof(Tab) == 40, "Synthetic native tab record.");

struct Actions {
    void** table = nullptr;
    Tab* first = nullptr;
    Tab* last = nullptr;
    void* selected = nullptr;
    bool allowed = true;
    bool fail = false;
    bool cancel = false;
    unsigned selections = 0;
    HWND destroy_on_select = nullptr;
    quickky::NativeFrame* retire_on_select = nullptr;
    bool switch_on_restore = false;
};

ULONG STDMETHODCALLTYPE Release(void*) { return 1; }
HRESULT STDMETHODCALLTYPE Select(void* self, GUID id)
{
    auto& actions = *static_cast<Actions*>(self);
    ++actions.selections;
    if (actions.fail) return E_FAIL;
    for (auto tab = actions.first; tab != actions.last; ++tab) {
        if (InlineIsEqualGUID(tab->id, id)) {
            actions.selected = tab->browser;
            if (actions.cancel) actions.allowed = false;
            if (actions.retire_on_select)
                quickky::NativeFrameTestAccess::Available(*actions.retire_on_select, {});
            if (actions.destroy_on_select) DestroyWindow(actions.destroy_on_select);
            return S_OK;
        }
    }
    return E_INVALIDARG;
}

LRESULT CALLBACK RootProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_SIZE && wparam != SIZE_MINIMIZED) {
        auto* actions = reinterpret_cast<Actions*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (actions && actions->switch_on_restore) {
            // Model native/user UI work processed reentrantly by SW_RESTORE.
            actions->switch_on_restore = false;
            actions->selected = actions->first->browser;
        }
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

struct Root {
    HWND window = CreateWindowW(L"QTabs.NativeFrameTest", L"QTabs native restore test",
        WS_OVERLAPPEDWINDOW, -30000, -30000, 320, 240, nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    ~Root()
    {
        if (IsWindow(window)) {
            SetWindowLongPtrW(window, GWLP_USERDATA, 0);
            DestroyWindow(window);
        }
    }
};

void TestReveal(bool maximized)
{
    Root root;
    Check(root.window != nullptr, "Create a synthetic window, not an Explorer root");
    void* methods[13]{};
    methods[2] = reinterpret_cast<void*>(&Release);
    methods[5] = reinterpret_cast<void*>(&Select);
    int first_browser = 1, second_browser = 2, object = 0;
    Tab tabs[]{{&first_browser, nullptr, {1}, 0}, {&second_browser, nullptr, {2}, 0}};
    Actions actions{methods, tabs, tabs + 2, &first_browser};
    SetWindowLongPtrW(root.window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&actions));
    quickky::NativeAbiLayout layout;
    layout.tabs = offsetof(Actions, first);
    layout.active = offsetof(Actions, selected);
    auto frame = quickky::NativeFrameTestAccess::Create(root.window, &object, &actions,
        methods, layout, [&actions] { return actions.allowed; });
    ShowWindow(root.window, maximized ? SW_SHOWMAXIMIZED : SW_SHOWNOACTIVATE);
    WINDOWPLACEMENT before{sizeof(before)};
    Check(GetWindowPlacement(root.window, &before) != FALSE, "Read original placement");
    ShowWindow(root.window, SW_MINIMIZE);
    Check(IsIconic(root.window) != FALSE, "Minimize the synthetic destination");

    Check(FAILED(frame->Reveal(GUID{99})) && actions.selections == 0 && IsIconic(root.window),
        "A missing transferred GUID must not select or restore the destination");
    actions.allowed = false;
    Check(FAILED(frame->Reveal(tabs[1].id)) && actions.selections == 0 && IsIconic(root.window),
        "A canceled lease must not select or restore");
    actions.allowed = true;
    HRESULT wrong_thread = S_OK;
    std::thread worker([&] { wrong_thread = frame->Reveal(tabs[1].id); });
    worker.join();
    Check(FAILED(wrong_thread) && actions.selections == 0 && IsIconic(root.window),
        "Only the owning UI thread may select or restore");
    actions.fail = true;
    Check(FAILED(frame->Reveal(tabs[1].id)) && IsIconic(root.window),
        "A failed selection must not restore");
    actions.fail = false;
    actions.cancel = true;
    Check(FAILED(frame->Reveal(tabs[1].id)) && IsIconic(root.window),
        "Cancellation during selection must prevent restore");
    actions.cancel = false;
    actions.allowed = true;
    actions.retire_on_select = frame.get();
    Check(FAILED(frame->Reveal(tabs[1].id)) && IsIconic(root.window),
        "Detachment clearing the lease callback during selection must prevent restore");
    actions.retire_on_select = nullptr;
    quickky::NativeFrameTestAccess::Available(*frame, [&actions] { return actions.allowed; });
    actions.switch_on_restore = true;
    const auto switched = frame->Reveal(tabs[1].id);
    Check(!actions.switch_on_restore && !IsIconic(root.window) && actions.selected == &first_browser,
        "Deliver the controlled tab switch inside the restore call");
    Check(FAILED(switched),
        "A tab switch during restore must stop foreground activation and success reporting");
    ShowWindow(root.window, SW_MINIMIZE);
    actions.selected = &first_browser;
    Check(frame->Reveal(tabs[1].id) == S_OK && !IsIconic(root.window) &&
        (IsZoomed(root.window) != FALSE) == maximized && actions.selected == &second_browser,
        "Restore the previous normal/maximized state and select the transferred tab");
    WINDOWPLACEMENT after{sizeof(after)};
    Check(GetWindowPlacement(root.window, &after) &&
        EqualRect(&before.rcNormalPosition, &after.rcNormalPosition), "Preserve normal placement");
    const auto selections = actions.selections;
    Check(frame->Reveal(tabs[0].id) == S_FALSE && actions.selections == selections,
        "Do not change selection or presentation on an already restored window");
    ShowWindow(root.window, SW_MINIMIZE);
    actions.destroy_on_select = root.window;
    Check(FAILED(frame->Reveal(tabs[1].id)) && !IsWindow(root.window),
        "Window destruction during selection must prevent restore/activation");
    Check(FAILED(frame->Reveal(tabs[1].id)), "Do not act on a destroyed target");
}
}

int main()
{
    WNDCLASSW type{};
    type.lpfnWndProc = RootProcedure;
    type.cbWndExtra = sizeof(void*);
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = L"QTabs.NativeFrameTest";
    if (!RegisterClassW(&type)) return EXIT_FAILURE;
    try {
        TestReveal(false);
        TestReveal(true);
        UnregisterClassW(type.lpszClassName, type.hInstance);
        std::cout << "Synthetic native-frame restore checks passed.\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        UnregisterClassW(type.lpszClassName, type.hInstance);
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

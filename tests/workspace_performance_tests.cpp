#include "group/workspace_controller.h"
#include "chrome/tab_strip.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace quickky {
struct WorkspaceControllerTestAccess {
    static void Native(WorkspaceController& controller, bool native) { controller.native_ = native; }
    static void Add(WorkspaceController& controller, NativeWindow window)
    {
        // Unprepared clients and an uninitialized probe keep the fixture free of IPC/COM.
        controller.entries_.emplace(window, WorkspaceController::Entry{std::make_unique<HookClient>(), L"Kept"});
    }
    static bool Contains(const WorkspaceController& controller, NativeWindow window)
    { return controller.entries_.count(window) != 0; }
    static auto Snapshot(const WorkspaceController& controller) { return controller.SnapshotAttachments(); }
    static void Reconcile(WorkspaceController& controller, const FolderLocations& locations,
        const std::map<NativeWindow, WindowIdentity>& attachments)
    { controller.ReconcileEntries(locations, attachments); }
    static void Release(WorkspaceController& controller, NativeWindow window, const WindowIdentity& identity)
    { controller.ReleaseIfCurrent(window, identity); }
};

struct TabStripTestAccess {
    static HWND Window(const TabStrip& strip) { return strip.window_; }
    static bool DeviceResources(const TabStrip& strip) { return strip.target_ && strip.brush_; }
    static bool Resources(TabStrip& strip) { return strip.Resources(); }
    static bool TextFormat(const TabStrip& strip) { return strip.format_ != nullptr; }
    static int TabWidth(const TabStrip& strip) { return static_cast<int>(strip.tab_width_); }
    static bool Hover(TabStrip& strip, int x, int y) { return strip.UpdateHover(x, y); }
    static D2D1_COLOR_F Foreground(const TabStrip& strip) { return strip.foreground_; }
    static void ClearForeground(TabStrip& strip) { strip.foreground_ = {}; }
};
}

namespace {
void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void TestFolderBatchLifetimes()
{
    using Access = quickky::WorkspaceControllerTestAccess;
    for (const bool native : {false, true}) {
        quickky::GroupModel group;
        quickky::CaptureCoordinator capture(group);
        quickky::ExplorerWindowProbe probe;
        quickky::HookLibrary library;
        quickky::WorkspaceController controller(nullptr, capture, group, probe, library, L"");
        Access::Native(controller, native);
        constexpr quickky::NativeWindow window = 42;
        quickky::FolderLocations locations;

        const auto before_resolution = Access::Snapshot(controller);
        Access::Add(controller, window); // Simulate attachment while automation pumps.
        Access::Reconcile(controller, locations, before_resolution);
        Check(Access::Contains(controller, window), "An uncovered late attachment survives the outer pass");

        const auto tracked = Access::Snapshot(controller);
        Access::Reconcile(controller, locations, tracked);
        Check(Access::Contains(controller, window), "Missing folder coverage defers rather than rejects");

        locations.emplace(reinterpret_cast<HWND>(window), std::vector<quickky::FolderLocation>{});
        Access::Reconcile(controller, locations, before_resolution);
        Check(Access::Contains(controller, window),
            "A candidate requested before resolution can become an attachment without inheriting its batch");

        auto replaced = tracked;
        ++replaced.at(window).generation;
        Access::Reconcile(controller, locations, replaced);
        Check(Access::Contains(controller, window), "An old generation cannot reject its replacement");
        Access::Release(controller, window, replaced.at(window));
        Check(Access::Contains(controller, window), "A stale deferred removal preserves a replacement");

        // A later pass with matching identity/coverage still performs validation.
        // The uninitialized probe rejects this unattached sentinel without COM.
        Access::Reconcile(controller, locations, tracked);
        Check(!Access::Contains(controller, window), "Fresh covered validation is not permanently deferred");
    }
}

void TestPublication()
{
    quickky::WorkspacePublicationState first, second;
    quickky::GuardRequest view;
    view.count = 2;
    view.active = view.tabs[0].id = 1;
    view.tabs[1].id = 2;
    wcscpy_s(view.tabs[0].title, L"Alpha");
    wcscpy_s(view.tabs[1].title, L"Beta");
    WINDOWPLACEMENT placement{sizeof(placement)};
    placement.showCmd = SW_SHOWNORMAL;
    placement.rcNormalPosition = {10, 20, 800, 600};
    unsigned configurations = 0, presentations = 0;
    auto publish = [&](quickky::WorkspacePublicationState& state, bool visible, bool actual_visible) {
        if (state.NeedsConfigure(view)) { ++configurations; state.Configured(view); }
        if (state.NeedsPresent(placement, visible, actual_visible)) {
            ++presentations; state.Presented(placement, visible);
        }
    };
    publish(first, true, true);
    publish(second, false, true);
    Check(configurations == 2 && presentations == 2, "Initial publication includes both attachments");
    for (unsigned pass = 0; pass < 100; ++pass) {
        publish(first, true, true);
        publish(second, false, false);
    }
    Check(configurations == 2 && presentations == 2, "Unchanged passes send no Configure or Present");
    // Identity/transport fields are not view content.
    view.target = 123;
    view.command = quickky::GuardCommand::Configure;
    Check(!first.NeedsConfigure(view), "Ignore non-view protocol fields");
    wcscpy_s(view.tabs[1].title, L"Renamed");
    publish(first, true, true);
    publish(second, false, false);
    Check(configurations == 4 && presentations == 2, "A changed label configures without placement work");
    placement.rcNormalPosition.right += 100;
    publish(first, true, true);
    publish(second, false, false);
    Check(presentations == 3, "Placement changes do not republish hidden members");
    view.active = 2;
    publish(second, true, false); // Destination first, as in WorkspaceController::Publish.
    publish(first, false, true);
    Check(configurations == 6 && presentations == 5, "Selection publishes and switches both members");
    Check(first.NeedsPresent(placement, false, true), "Correct externally shown inactive root");
    Check(second.NeedsPresent(placement, true, false), "Correct externally hidden active root");
    placement.showCmd = SW_SHOWMAXIMIZED;
    Check(second.NeedsPresent(placement, true, true), "Maximize change requires presentation");
    quickky::WorkspacePublicationState replacement;
    Check(replacement.NeedsConfigure(view) && replacement.NeedsPresent(placement, true, true),
        "Replacement attachment never inherits publication cache");
    // Failure is not acknowledgment: do not record until the send succeeds.
    Check(replacement.NeedsConfigure(view) && replacement.NeedsPresent(placement, true, true),
        "Unacknowledged work remains pending");
    view.tabs[0].id = 99;
    Check(second.NeedsConfigure(view), "Changed tab identity requires configuration");
    view.count = 1;
    Check(second.NeedsConfigure(view), "Changed count requires configuration");
}

void TestStripResourcesAndHover()
{
    WNDCLASSW definition{};
    definition.hInstance = GetModuleHandleW(nullptr);
    definition.lpfnWndProc = DefWindowProcW;
    definition.lpszClassName = L"QTabs.PerformanceFixture";
    Check(RegisterClassW(&definition) != 0, "Register hidden fixture");
    const auto parent = CreateWindowExW(0, definition.lpszClassName, L"", WS_OVERLAPPEDWINDOW,
        0, 0, 900, 400, nullptr, nullptr, definition.hInstance, nullptr);
    Check(parent != nullptr, "Create hidden test-owned parent");
    struct Cleanup { HWND window; ~Cleanup() { DestroyWindow(window); } } cleanup{parent};
    const auto owner = std::make_shared<int>(0);
    quickky::TabStrip strip;
    Check(strip.Create(parent, [](quickky::TabInput, std::uint64_t) {}, owner), "Create synthetic strip");
    using Access = quickky::TabStripTestAccess;
    const auto window = Access::Window(strip);
    quickky::GuardRequest view;
    view.count = 2;
    view.active = view.tabs[0].id = 1;
    view.tabs[1].id = 2;
    Check(strip.Configure(view), "Configure synthetic tabs");
    // Hidden HWNDs may discard invalid regions. Exercise the same visual-change
    // predicate used by WM_MOUSEMOVE/WM_MOUSELEAVE, without displaying a fixture.
    Check(Access::Hover(strip, 10, 10), "Entering a tab changes visual hover");
    Check(!Access::Hover(strip, 20, 10), "Moving inside the same tab does not repaint");
    Check(Access::Hover(strip, Access::TabWidth(strip) + 10, 10), "Crossing tabs changes visual hover");
    Check(Access::Hover(strip, -1, -1), "Leaving a hovered tab changes visual hover");
    Check(!Access::Hover(strip, -1, -1), "Repeated mouse leave does not repaint");
    Check(Access::DeviceResources(strip), "Initial render resources exist");
    strip.Hide(true);
    Check(!Access::DeviceResources(strip) && Access::TextFormat(strip),
        "Inactive strip releases device resources but retains text format");
    SendMessageW(window, WM_PAINT, 0, 0);
    Check(!Access::DeviceResources(strip), "Hidden paint cannot recreate render resources");
    Check(Access::Resources(strip), "Render resources can be recreated lazily");
    strip.Hide();
    Check(Access::DeviceResources(strip), "Transient layout hide avoids device-resource churn");
    Access::ClearForeground(strip);
    SendMessageW(window, WM_SYSCOLORCHANGE, 0, 0);
    Check(Access::Foreground(strip).a == 1.0f, "System color notification refreshes cached theme");
}
}

int main()
{
    try {
        TestFolderBatchLifetimes();
        TestPublication();
        TestStripResourcesAndHover();
        std::cout << "PASS: publication operation counts and hidden synthetic strip resources/hover\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}

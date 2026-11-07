#include "capture/explorer_discovery.h"
#include "capture/explorer_window_probe.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace quickky {
struct ExplorerDiscoveryTestAccess {
    static bool BeginWithoutHooks(ExplorerDiscovery& discovery)
    {
        if (ExplorerDiscovery::current_) return false;
        ExplorerDiscovery::current_ = &discovery;
        return discovery.Refresh();
    }
    static void Deliver(DWORD event, HWND window)
    { ExplorerDiscovery::WinEvent(nullptr, event, window, OBJID_WINDOW, CHILDID_SELF, 0, 0); }
    static UINT_PTR Timer(const ExplorerDiscovery& discovery) { return discovery.reconcile_timer_; }
    static void Fire(UINT_PTR timer) { ExplorerDiscovery::Reconcile(nullptr, WM_TIMER, timer, 0); }
};
}

namespace {

void Check(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}

struct FixtureWindow {
    explicit FixtureWindow(const wchar_t* name = L"CabinetWClass")
        : value(CreateWindowExW(0, name, L"", 0,
            0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr)) {}
    HWND value;
    ~FixtureWindow() { if (value != nullptr) { DestroyWindow(value); } }
    void Destroy() { Check(DestroyWindow(value) != FALSE, "destroy fixture"); value = nullptr; }
};

template<class Condition>
void PumpUntil(Condition condition)
{
    const auto deadline = GetTickCount64() + 3000;
    while (!condition()) {
        const auto now = GetTickCount64();
        Check(now < deadline, "timed out waiting for a WinEvent");
        const auto remaining = static_cast<DWORD>(deadline - now);
        Check(MsgWaitForMultipleObjectsEx(0, nullptr, remaining, QS_ALLINPUT,
            MWMO_INPUTAVAILABLE) != WAIT_FAILED, "message wait");
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

void TestCoalescing()
{
    using Access = quickky::ExplorerDiscoveryTestAccess;
    FixtureWindow fixture;
    Check(fixture.value != nullptr, "Create controlled coalescing fixture");
    unsigned inspections = 0;
    std::vector<quickky::WindowChange> changes;
    quickky::ExplorerDiscovery discovery([&](HWND window) -> std::optional<quickky::WindowCandidate> {
        if (window != fixture.value) return std::nullopt;
        ++inspections;
        return quickky::WindowCandidate{reinterpret_cast<quickky::NativeWindow>(window),
            GetCurrentProcessId(), GetCurrentThreadId(), 1};
    }, [&](const quickky::WindowChange& change) { changes.push_back(change); });
    // No global hooks and no message pumping in this phase. Feed the real event
    // callback, then explicitly consume its timer so delivery and counts have
    // exact boundaries even when another process generates CabinetWClass events.
    Check(Access::BeginWithoutHooks(discovery), "Start controlled discovery state");
    const auto before_burst = inspections;
    Access::Deliver(EVENT_OBJECT_SHOW, fixture.value);
    const auto burst_timer = Access::Timer(discovery);
    Check(burst_timer != 0, "A lifecycle event schedules deferred work");
    for (unsigned i = 1; i < 64; ++i) {
        Access::Deliver(EVENT_OBJECT_SHOW, fixture.value);
        Check(Access::Timer(discovery) == burst_timer, "Events reuse the pending timer");
    }
    Check(inspections == before_burst, "Event callbacks do not enumerate immediately");
    Access::Fire(burst_timer);
    Check(!discovery.failed() && inspections == before_burst + 1 && Access::Timer(discovery) == 0,
        "One pending lifecycle batch performs exactly one deferred enumeration");
    Access::Fire(burst_timer);
    Check(inspections == before_burst + 1, "A consumed timer callback does not enumerate again");

    // An event delivered after consumption is a new batch, not a duplicate of
    // the producer's earlier notification loop.
    Access::Deliver(EVENT_OBJECT_SHOW, fixture.value);
    const auto later_timer = Access::Timer(discovery);
    Check(later_timer != 0, "Later delivery schedules another pass");
    Access::Fire(later_timer);
    Check(inspections == before_burst + 2, "A later delivered batch legitimately enumerates again");

    FixtureWindow unrelated(L"STATIC");
    Check(unrelated.value != nullptr, "Create unrelated hidden fixture");
    const auto before_foreground = inspections;
    for (unsigned i = 0; i < 32; ++i) Access::Deliver(EVENT_SYSTEM_FOREGROUND, unrelated.value);
    Check(Access::Timer(discovery) == 0 && inspections == before_foreground,
        "Unrelated foreground events neither schedule nor enumerate");

    Access::Deliver(EVENT_OBJECT_SHOW, fixture.value);
    const auto explicit_timer = Access::Timer(discovery);
    Check(explicit_timer != 0 && discovery.Refresh(), "Explicit refresh consumes pending work");
    const auto after_explicit = inspections;
    Check(Access::Timer(discovery) == 0, "Explicit refresh cancels its scheduled timer");
    Access::Fire(explicit_timer);
    Check(inspections == after_explicit, "The canceled callback adds no scan after explicit refresh");

    const auto before_destroy = changes.size();
    const auto before_rediscovery = inspections;
    Access::Deliver(EVENT_OBJECT_DESTROY, fixture.value);
    const auto destroy_timer = Access::Timer(discovery);
    Check(destroy_timer != 0 && changes.size() == before_destroy + 1 &&
        changes.back().kind == quickky::WindowChangeKind::Removed && discovery.windows().empty() &&
        inspections == before_rediscovery, "Destroy retires identity immediately without enumerating");
    Access::Deliver(EVENT_OBJECT_SHOW, fixture.value);
    Check(Access::Timer(discovery) == destroy_timer, "Destroy/show share pending reconciliation");
    Access::Fire(destroy_timer);
    Check(changes.size() == before_destroy + 2 &&
        changes.back().kind == quickky::WindowChangeKind::Added && inspections == before_rediscovery + 1,
        "Deferred rediscovery follows removal of a still-live/reused handle");

    Access::Deliver(EVENT_OBJECT_SHOW, fixture.value);
    const auto stop_timer = Access::Timer(discovery);
    Check(stop_timer != 0, "Stop fixture has pending deferred work");
    discovery.Stop();
    const auto after_stop = inspections;
    Check(Access::Timer(discovery) == 0, "Stop cancels pending timer state");
    Access::Fire(stop_timer);
    Access::Deliver(EVENT_OBJECT_SHOW, fixture.value);
    Check(inspections == after_stop && discovery.windows().empty(),
        "Stopped discovery ignores pending callbacks and late events");
}

void Run()
{
    WNDCLASSW fixture_class{};
    fixture_class.lpfnWndProc = DefWindowProcW;
    fixture_class.hInstance = GetModuleHandleW(nullptr);
    fixture_class.lpszClassName = L"CabinetWClass";
    Check(RegisterClassW(&fixture_class) != 0, "register fixture class");

    FixtureWindow first;
    Check(first.value != nullptr, "create initial fixture");
    std::set<HWND> accepted{first.value};
    std::uint64_t process_started_at = 1;
    auto inspect = [&](HWND window) -> std::optional<quickky::WindowCandidate> {
        if (accepted.count(window) == 0 || !IsWindow(window)) { return std::nullopt; }
        DWORD process = 0;
        const auto thread = GetWindowThreadProcessId(window, &process);
        return quickky::WindowCandidate{reinterpret_cast<quickky::NativeWindow>(window),
            process, thread, process_started_at};
    };
    std::vector<quickky::WindowChange> changes;
    bool retry_requested = false;
    quickky::ExplorerDiscovery discovery(inspect,
        [&](const quickky::WindowChange& change) { changes.push_back(change); },
        [&] { retry_requested = true; });
    Check(discovery.Start(), "start discovery");
    Check(discovery.windows().size() == 1 && changes.size() == 1, "initial enumeration");
    Check(changes[0].kind == quickky::WindowChangeKind::Added, "initial add event");
    Check(discovery.Refresh() && discovery.Refresh() && changes.size() == 1, "coalesce duplicate scans");

    quickky::ExplorerDiscovery duplicate(inspect, [](const quickky::WindowChange&) {});
    Check(!duplicate.Start(), "reject two observers on the same message thread");

    FixtureWindow second;
    Check(second.value != nullptr, "create second fixture");
    accepted.insert(second.value);
    PumpUntil([&] { return discovery.windows().size() == 2 || discovery.failed(); });
    Check(!discovery.failed() && changes.size() == 2, "observe creation notification");

    // Drain the create event while the inspector still rejects this identity.
    // Becoming ready later emits no new lifecycle event; a deferred discovery
    // pass must admit it without waiting for one-second periodic reconciliation.
    retry_requested = false;
    FixtureWindow delayed;
    Check(delayed.value != nullptr, "create not-yet-ready fixture");
    PumpUntil([&] { return retry_requested || discovery.failed(); });
    const auto delayed_handle = reinterpret_cast<quickky::NativeWindow>(delayed.value);
    Check(!discovery.failed() && discovery.windows().count(delayed_handle) == 0,
        "Readiness hint is not admission");
    accepted.insert(delayed.value);
    const auto before_ready = changes.size();
    Check(discovery.Refresh() && discovery.windows().count(delayed_handle) == 1 &&
        changes.size() == before_ready + 1, "Deferred re-probe admits ready identity without another event");
    accepted.erase(delayed.value);
    delayed.Destroy();
    PumpUntil([&] { return discovery.windows().size() == 2 || discovery.failed(); });
    Check(!discovery.failed(), "Remove delayed fixture");
    const auto before_first_remove = changes.size();

    accepted.erase(first.value);
    first.Destroy();
    PumpUntil([&] { return discovery.windows().size() == 1 || discovery.failed(); });
    Check(!discovery.failed() && changes.size() == before_first_remove + 1 &&
        changes.back().kind == quickky::WindowChangeKind::Removed,
        "observe destruction notification");

    const auto before_replacement = changes.size();
    ++process_started_at;
    Check(discovery.Refresh(), "reconcile replacement process identity");
    Check(changes.size() == before_replacement + 2
        && changes[before_replacement].kind == quickky::WindowChangeKind::Removed
        && changes.back().kind == quickky::WindowChangeKind::Added, "replacement removes before adding");

    accepted.erase(second.value);
    Check(discovery.Refresh() && discovery.windows().empty(), "remove ineligible live window");
    Check(IsWindow(second.value) != FALSE, "discovery must not close a window");
    accepted.insert(second.value);
    Check(discovery.Refresh() && discovery.windows().size() == 1, "rediscover eligible window");

    discovery.Stop();
    Check(discovery.windows().empty() && !discovery.Refresh(), "stopped observer cannot refresh");
    Check(duplicate.Start(), "release thread ownership after stop");
    duplicate.Stop();

    quickky::ExplorerDiscovery throwing_handler(inspect,
        [](const quickky::WindowChange&) { throw std::runtime_error("handler failure"); });
    Check(!throwing_handler.Start() && throwing_handler.failed(), "contain handler exception");
    quickky::ExplorerDiscovery throwing_probe(
        [](HWND) -> std::optional<quickky::WindowCandidate> { throw std::runtime_error("probe failure"); },
        [](const quickky::WindowChange&) {});
    Check(!throwing_probe.Start() && throwing_probe.failed(), "contain enumeration exception");
    Check(discovery.Start(), "restart after failed observers");
    discovery.Stop();

    quickky::ExplorerDiscovery throwing_retry(inspect, [](const quickky::WindowChange&) {},
        [] { throw std::runtime_error("retry notification failure"); });
    Check(throwing_retry.Start(), "start throwing retry fixture");
    FixtureWindow rejected;
    Check(rejected.value != nullptr, "create rejected retry fixture");
    PumpUntil([&] { return throwing_retry.failed(); });
    Check(IsWindow(rejected.value) != FALSE, "Retry notification never mutates rejected window");
    throwing_retry.Stop();

    TestCoalescing();

    quickky::ExplorerWindowProbe production;
    Check(production.Initialize(), "initialize production probe");
    Check(!production.Inspect(nullptr), "reject null handle");
    Check(!production.Inspect(second.value), "reject hidden non-Explorer CabinetWClass fixture");
    Check(!production.Inspect(GetDesktopWindow()), "reject desktop");
    Check(!production.Inspect(FindWindowW(L"Shell_TrayWnd", nullptr)), "reject taskbar");
}

} // namespace

int main()
{
    const auto apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(apartment)) { return EXIT_FAILURE; }
    int result = EXIT_SUCCESS;
    try {
        Run();
        std::cout << "PASS: read-only discovery lifecycle, event delivery, reconciliation, and failure containment\n";
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        result = EXIT_FAILURE;
    }
    CoUninitialize();
    return result;
}

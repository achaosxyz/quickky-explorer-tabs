#include "capture/explorer_discovery.h"
#include "capture/explorer_window_probe.h"

#include <windows.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>
#include <vector>

namespace {

void Check(bool condition, const char* message)
{
    if (!condition) { throw std::runtime_error(message); }
}

struct FixtureWindow {
    HWND value = CreateWindowExW(0, L"CabinetWClass", L"", 0,
        0, 0, 1, 1, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
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
    quickky::ExplorerDiscovery discovery(inspect,
        [&](const quickky::WindowChange& change) { changes.push_back(change); });
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

    accepted.erase(first.value);
    first.Destroy();
    PumpUntil([&] { return discovery.windows().size() == 1 || discovery.failed(); });
    Check(!discovery.failed() && changes.back().kind == quickky::WindowChangeKind::Removed,
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

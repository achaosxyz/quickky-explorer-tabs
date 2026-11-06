#include "capture/capture_coordinator.h"
#include "capture/explorer_discovery.h"
#include "capture/explorer_window_probe.h"

#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>

#if !defined(_M_X64)
#error Quickky Explorer Tabs requires an x86_64 target.
#endif

namespace {

constexpr wchar_t kWindowClass[] = L"QuickkyExplorerTabs.Controller";
constexpr wchar_t kMutexName[] = L"Local\\QuickkyExplorerTabs.Controller";
constexpr wchar_t kStopName[] = L"Local\\QuickkyExplorerTabs.Stop";
constexpr UINT_PTR kRefreshTimer = 1;
constexpr UINT_PTR kExitTimer = 2;
using Handle = std::unique_ptr<void, decltype(&CloseHandle)>;

enum class Mode { Resident, Snapshot, Observe, Smoke, Stop };

struct Options {
    Mode mode = Mode::Resident;
    UINT duration = 0;
};

struct Apartment {
    HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~Apartment() { if (SUCCEEDED(result)) { CoUninitialize(); } }
};

std::optional<Options> ParseOptions(int count, wchar_t** arguments)
{
    if (count == 1) {
        return Options{};
    }
    if (count == 2) {
        const std::wstring_view option(arguments[1]);
        if (option == L"--smoke-test") { return Options{Mode::Smoke}; }
        if (option == L"--snapshot") { return Options{Mode::Snapshot}; }
        if (option == L"--stop") { return Options{Mode::Stop}; }
    }
    if (count == 3 && std::wstring_view(arguments[1]) == L"--observe-ms") {
        const std::wstring_view value(arguments[2]);
        UINT duration = 0;
        for (const auto digit : value) {
            if (digit < L'0' || digit > L'9') { return std::nullopt; }
            duration = duration * 10 + static_cast<UINT>(digit - L'0');
            if (duration > 600000) { return std::nullopt; }
        }
        if (duration != 0) { return Options{Mode::Observe, duration}; }
    }
    return std::nullopt;
}

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_DESTROY) {
        PostQuitMessage(EXIT_SUCCESS);
        return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}

bool WriteSnapshot(const quickky::ExplorerDiscovery& discovery,
    const quickky::CaptureCoordinator& capture, const quickky::GroupModel& group)
{
    std::ostringstream output;
    output << "{\"mode\":\"read-only\",\"managed_tabs\":" << group.tabs().size() << ",\"windows\":[";
    bool first = true;
    for (const auto& item : discovery.windows()) {
        const auto record = capture.Find(item.first);
        if (!first) { output << ','; }
        first = false;
        output << "{\"hwnd\":\"0x" << std::hex << item.first << std::dec
               << "\",\"pid\":" << item.second.process_id
               << ",\"tid\":" << item.second.thread_id
               << ",\"generation\":" << (record ? record->window.generation : 0) << '}';
    }
    output << "]}\n";
    const auto text = output.str();
    const auto handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    std::size_t offset = 0;
    while (offset < text.size()) {
        DWORD written = 0;
        if (!WriteFile(handle, text.data() + offset,
            static_cast<DWORD>(text.size() - offset), &written, nullptr) || written == 0) {
            return false;
        }
        offset += written;
    }
    return true;
}

int Run(HINSTANCE instance, const Options& options)
{
    if (options.mode == Mode::Stop) {
        const Handle stop(OpenEventW(EVENT_MODIFY_STATE, FALSE, kStopName), CloseHandle);
        if (!stop) { return GetLastError() == ERROR_FILE_NOT_FOUND ? EXIT_SUCCESS : EXIT_FAILURE; }
        return SetEvent(stop.get()) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    Handle singleton(nullptr, CloseHandle);
    Handle stop(nullptr, CloseHandle);
    if (options.mode == Mode::Resident) {
        singleton.reset(CreateMutexW(nullptr, FALSE, kMutexName));
        const auto error = GetLastError();
        if (!singleton) { return EXIT_FAILURE; }
        if (error == ERROR_ALREADY_EXISTS) { return EXIT_SUCCESS; }
        stop.reset(CreateEventW(nullptr, TRUE, FALSE, kStopName));
        if (!stop || !ResetEvent(stop.get())) { return EXIT_FAILURE; }
    }

    const Apartment apartment;
    if (FAILED(apartment.result)) { return EXIT_FAILURE; }

    WNDCLASSW window_class{};
    window_class.lpfnWndProc = WindowProcedure;
    window_class.hInstance = instance;
    window_class.lpszClassName = kWindowClass;
    if (RegisterClassW(&window_class) == 0) { return EXIT_FAILURE; }

    const HWND window = CreateWindowExW(0, kWindowClass, L"", 0,
        0, 0, 0, 0, HWND_MESSAGE, nullptr, instance, nullptr);
    if (window == nullptr) { return EXIT_FAILURE; }

    quickky::GroupModel group;
    quickky::CaptureCoordinator capture(group);
    quickky::ExplorerWindowProbe probe;
    quickky::ExplorerDiscovery discovery(
        [&probe](HWND candidate) { return probe.Inspect(candidate); },
        [&capture](const quickky::WindowChange& change) {
            if (change.kind == quickky::WindowChangeKind::Added) {
                capture.Observe(change.window);
            } else if (const auto record = capture.Find(change.window.handle);
                record != nullptr && record->window.window == change.window) {
                const auto identity = record->window;
                if (change.kind == quickky::WindowChangeKind::Removed) {
                    capture.Forget(identity);
                } else {
                    capture.OnForeground(identity);
                }
            }
        });

    if (options.mode == Mode::Smoke) {
        if (!PostMessageW(window, WM_CLOSE, 0, 0)) { return EXIT_FAILURE; }
    } else {
        if (!probe.Initialize() || !discovery.Start()) { return EXIT_FAILURE; }
        if (options.mode == Mode::Snapshot) {
            return WriteSnapshot(discovery, capture, group) ? EXIT_SUCCESS : EXIT_FAILURE;
        }
        if (SetTimer(window, kRefreshTimer, 1000, nullptr) == 0) { return EXIT_FAILURE; }
        if (options.mode == Mode::Observe
            && SetTimer(window, kExitTimer, options.duration, nullptr) == 0) {
            return EXIT_FAILURE;
        }
    }

    const HANDLE stop_handle = stop.get();
    const DWORD handle_count = stop_handle != nullptr ? 1 : 0;
    for (;;) {
        const auto result = MsgWaitForMultipleObjectsEx(handle_count,
            handle_count ? &stop_handle : nullptr, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
        if (result == WAIT_FAILED) { return EXIT_FAILURE; }
        if (handle_count != 0 && result == WAIT_OBJECT_0) { return EXIT_SUCCESS; }

        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { return static_cast<int>(message.wParam); }
            if (message.hwnd == window && message.message == WM_TIMER) {
                if (message.wParam == kRefreshTimer && !discovery.Refresh()) { return EXIT_FAILURE; }
                if (message.wParam == kExitTimer) { DestroyWindow(window); }
            } else {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (discovery.failed()) { return EXIT_FAILURE; }
        }
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    try {
        int count = 0;
        auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
        if (arguments == nullptr) { return EXIT_FAILURE; }
        const auto options = ParseOptions(count, arguments);
        LocalFree(arguments);
        return options ? Run(instance, *options) : ERROR_BAD_ARGUMENTS;
    } catch (...) {
        return EXIT_FAILURE;
    }
}

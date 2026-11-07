#include "capture/explorer_window_probe.h"
#include "common/hook_protocol.h"
#include "hook/hook_client.h"

#include <cstdlib>
#include <cwchar>
#include <iostream>
#include <memory>
#include <string>

namespace {
HANDLE stopped = nullptr;
BOOL WINAPI StopRequested(DWORD) { if (stopped) SetEvent(stopped); return TRUE; }
struct Apartment {
    HRESULT status = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~Apartment() { if (SUCCEEDED(status)) CoUninitialize(); }
};

DWORD_PTR Send(HWND target, HWND sender, quickky::GuardRequest& request)
{
    COPYDATASTRUCT data{quickky::kGuardProtocol, sizeof(request), &request};
    DWORD_PTR result = 0;
    if (!SendMessageTimeoutW(target, WM_COPYDATA, reinterpret_cast<WPARAM>(sender),
            reinterpret_cast<LPARAM>(&data), SMTO_ABORTIFHUNG, 2000, &result)) return 0;
    return result;
}
} // namespace

int wmain(int argc, wchar_t** argv)
{
    if (argc != 4) { std::cerr << "Usage: QuickkyNativeGuardProbe HWND PID SECONDS\n"; return 160; }
    wchar_t* end = nullptr;
    const auto handle = wcstoull(argv[1], &end, 0);
    if (!handle || *end) return 160;
    const auto expected_pid = wcstoul(argv[2], &end, 10);
    if (!expected_pid || *end) return 160;
    const auto seconds = wcstoul(argv[3], &end, 10);
    if (!seconds || seconds > 120 || *end) return 160;
    const HWND target = reinterpret_cast<HWND>(handle);
    Apartment apartment;
    if (FAILED(apartment.status)) return 1;
    int result = 1;
    {
        quickky::ExplorerWindowProbe probe;
        auto candidate = probe.Initialize() ? probe.Inspect(target) : std::nullopt;
        if (!candidate || candidate->process_id != expected_pid) {
            std::cerr << "Target identity/eligibility mismatch; no hook installed.\n";
            return 1;
        }
        quickky::HookLibrary library;
        if (!library.Load()) { std::cerr << "Hook DLL staging failed: " << GetLastError() << '\n'; return 1; }
        const auto module = library.module();
        const auto callback = reinterpret_cast<HOOKPROC>(GetProcAddress(module, "QuickkyBootstrap"));
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = DefWindowProcW;
        window_class.hInstance = GetModuleHandleW(nullptr);
        window_class.lpszClassName = L"Quickky.NativeGuardProbe";
        RegisterClassW(&window_class);
        const auto sender = CreateWindowW(window_class.lpszClassName, L"", 0, 0, 0, 0, 0,
            HWND_MESSAGE, nullptr, window_class.hInstance, nullptr);
        quickky::GuardRequest request;
        request.controller_pid = GetCurrentProcessId();
        request.target = handle;
        FILETIME created{}, exited{}, kernel{}, user{};
        GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user);
        request.controller_started_at = (static_cast<std::uint64_t>(created.dwHighDateTime) << 32)
            | created.dwLowDateTime;
        if (FAILED(CoCreateGuid(&request.nonce))) return 1;
        const auto hook = callback && sender ? SetWindowsHookExW(WH_CALLWNDPROC, callback, module,
            candidate->thread_id) : nullptr;
        if (!hook) { std::cerr << "Thread hook failed: " << GetLastError() << '\n'; return 1; }
        Send(target, sender, request);
        request.command = quickky::GuardCommand::Status;
        stopped = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        SetConsoleCtrlHandler(StopRequested, TRUE);
        auto status = Send(target, sender, request);
        if ((status & 1) != 0 && stopped) {
            result = 0;
            for (unsigned second = 0; second < seconds; ++second) {
                status = Send(target, sender, request);
                std::cout << "active=" << (status & 1) << " tabs=" << ((status >> 8) & 255)
                          << " blocked=" << (status >> 16) << std::endl;
                if (!(status & 1)) { result = 1; break; }
                if (WaitForSingleObject(stopped, 1000) != WAIT_TIMEOUT) break;
            }
        } else std::cerr << "Guard attachment rejected; Explorer left unmanaged.\n";
        request.command = quickky::GuardCommand::Detach;
        const auto detached = Send(target, sender, request);
        std::cout << "detached=" << detached << std::endl;
        if (!detached) result = 1;
        UnhookWindowsHookEx(hook);
        SetConsoleCtrlHandler(StopRequested, FALSE);
        if (stopped) CloseHandle(stopped);
        stopped = nullptr;
        DestroyWindow(sender);
    }
    return result;
}

#include <windows.h>
#include <shellapi.h>

#include <cstdlib>
#include <string_view>

#if !defined(_M_X64)
#error Quickky Explorer Tabs requires an x86_64 target.
#endif

namespace {

constexpr wchar_t kWindowClass[] = L"QuickkyExplorerTabs.Application";
constexpr wchar_t kWindowTitle[] = L"Quickky Explorer Tabs";

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_DESTROY) {
        PostQuitMessage(EXIT_SUCCESS);
        return 0;
    }

    return DefWindowProcW(window, message, wparam, lparam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show_command)
{
    int argument_count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    if (arguments == nullptr) {
        return EXIT_FAILURE;
    }

    const bool smoke_test = argument_count == 2
        && std::wstring_view(arguments[1]) == L"--smoke-test";
    const bool valid_arguments = argument_count == 1 || smoke_test;
    LocalFree(arguments);

    if (!valid_arguments) {
        return ERROR_BAD_ARGUMENTS;
    }

    WNDCLASSW window_class{};
    window_class.lpfnWndProc = WindowProcedure;
    window_class.hInstance = instance;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    window_class.lpszClassName = kWindowClass;

    if (RegisterClassW(&window_class) == 0) {
        return EXIT_FAILURE;
    }

    const HWND window = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        nullptr, nullptr, instance, nullptr);

    if (window == nullptr) {
        return EXIT_FAILURE;
    }

    if (smoke_test) {
        if (!PostMessageW(window, WM_CLOSE, 0, 0)) {
            DestroyWindow(window);
            return EXIT_FAILURE;
        }
    } else {
        ShowWindow(window, show_command);
    }

    MSG message{};
    for (;;) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result == -1) {
            DestroyWindow(window);
            return EXIT_FAILURE;
        }
        if (result == 0) {
            return static_cast<int>(message.wParam);
        }

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

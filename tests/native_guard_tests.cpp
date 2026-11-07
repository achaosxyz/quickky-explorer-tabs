#include "explorer/native_tab_guard.h"

#include <iostream>
#include <stdexcept>

namespace {
void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
HWND Create(const wchar_t* name, HWND parent = nullptr)
{
    return CreateWindowW(name, L"", parent ? WS_CHILD : WS_OVERLAPPEDWINDOW,
        0, 0, 1, 1, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
}
} // namespace

int main()
{
    HWND target = nullptr, other = nullptr;
    try {
        for (const auto name : {L"CabinetWClass", L"ShellTabWindowClass"}) {
            WNDCLASSW window_class{};
            window_class.hInstance = GetModuleHandleW(nullptr);
            window_class.lpfnWndProc = DefWindowProcW;
            window_class.lpszClassName = name;
            Check(RegisterClassW(&window_class) != 0, "Register synthetic class");
        }
        target = Create(L"CabinetWClass");
        other = Create(L"CabinetWClass");
        Check(target && other, "Create hidden test-owned roots");
        quickky::NativeTabGuard guard;
        Check(!guard.Start(target), "Zero-tab targets must be rejected");
        Check(Create(L"ShellTabWindowClass", target) != nullptr, "Create initial tab");
        Check(guard.Start(target), "Attach to exactly one synthetic tab");
        Check(Create(L"ShellTabWindowClass", target) == nullptr, "Reject additional tab");
        Check(guard.blocked() == 1 && guard.CountTabs(target) == 1, "Count rejection without extra tab");
        Check(Create(L"ShellTabWindowClass", other) != nullptr, "Do not affect another root");
        Check(Create(L"STATIC", target) != nullptr, "Do not affect other child classes");
        guard.Stop();
        Check(Create(L"ShellTabWindowClass", target) != nullptr, "Restore native creation after stop");
        Check(!guard.Start(target), "Existing multi-tab targets must be rejected");
        DestroyWindow(target);
        DestroyWindow(other);
        std::cout << "PASS: Synthetic guard creation, scope, rejection, and restoration.\n";
        return 0;
    } catch (const std::exception& error) {
        if (target) DestroyWindow(target);
        if (other) DestroyWindow(other);
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}

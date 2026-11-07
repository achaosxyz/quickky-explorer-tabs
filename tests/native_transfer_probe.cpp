#include "capture/explorer_window_probe.h"
#include "explorer/native_tab_guard.h"
#include "hook/hook_client.h"
#include <iostream>
#include <cwchar>

// Manual, directory-scoped probe; never registered with CTest. Both roots must
// contain only explicitly scoped folders before any attachment or mutation.
int wmain(int argc, wchar_t** argv)
{
    if (argc != 4) {
        std::cerr << "Usage: QuickkyNativeTransferProbe ROOT DESTINATION_HWND SOURCE_HWND\n";
        return 160;
    }
    if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
    struct Apartment { ~Apartment() { CoUninitialize(); } } apartment;
    auto root = quickky::NormalizeDirectory(argv[1]);
    wchar_t* end = nullptr;
    const auto destination = reinterpret_cast<HWND>(wcstoull(argv[2], &end, 10));
    if (!root || !destination || *end) return 160;
    const auto source = reinterpret_cast<HWND>(wcstoull(argv[3], &end, 10));
    if (!source || *end || source == destination) return 160;
    quickky::ExplorerWindowProbe probe;
    if (!probe.Initialize()) return 1;
    const auto eligible = [&](HWND window) {
        const auto folders = quickky::ResolveFolders(window);
        if (folders.empty() || folders.size() != quickky::NativeTabGuard::CountTabs(window)) return false;
        for (const auto& folder : folders) {
            if (!quickky::IsWithinDirectory(folder.parsing_name, *root)) return false;
        }
        return true;
    };
    const auto destination_id = probe.Inspect(destination);
    const auto source_id = probe.Inspect(source);
    if (!destination_id || !source_id || !eligible(destination) || !eligible(source)) {
        std::cerr << "Scope/identity check failed; nothing attached.\n"; return 1;
    }
    WNDCLASSW definition{};
    definition.hInstance = GetModuleHandleW(nullptr);
    definition.lpfnWndProc = DefWindowProcW;
    definition.lpszClassName = L"Quickky.NativeTransferProbe";
    RegisterClassW(&definition);
    const auto sender = CreateWindowW(definition.lpszClassName, L"", 0, 0, 0, 0, 0,
        HWND_MESSAGE, nullptr, definition.hInstance, nullptr);
    struct Endpoint { HWND window; ~Endpoint() { DestroyWindow(window); } } endpoint{sender};
    quickky::HookLibrary library;
    if (!sender || !library.Load()) return 1;
    quickky::HookClient main, incoming;
    const quickky::WindowIdentity main_identity{*destination_id, 1};
    const quickky::WindowIdentity source_identity{*source_id, 1};
    bool ok = main.Prepare(sender, library.module(), main_identity, quickky::WorkspaceMode::Native);
    std::cout << "main.prepare=" << ok << std::endl;
    if (!ok) return 1;
    ok = main.Commit();
    std::cout << "main.commit=" << ok << std::endl;
    if (!ok) return 1;
    ok = incoming.Prepare(sender, library.module(), source_identity, quickky::WorkspaceMode::Native) && incoming.Commit();
    std::cout << "source.commit=" << ok << std::endl;
    if (!ok) return 1;
    std::vector<GUID> tabs;
    GUID active{};
    quickky::OwnedPidl folder;
    ok = incoming.ReadNative(tabs, active, folder);
    std::cout << "source.snapshot=" << ok << " count=" << tabs.size() << std::endl;
    if (!ok || !(probe.Inspect(destination) == destination_id) || !(probe.Inspect(source) == source_id) ||
        !eligible(destination) || !eligible(source)) return 1;
    GUID created{};
    ok = main.AddNative(folder.get(), created);
    std::cout << "destination.add=" << ok << std::endl;
    if (!ok) return 1;
    GUID selected{};
    quickky::OwnedPidl received;
    ok = main.ReadNative(tabs, selected, received) && InlineIsEqualGUID(created, selected) &&
        ILIsEqual(folder.get(), received.get());
    std::cout << "destination.confirm=" << ok << std::endl;
    if (!ok) return 1;
    ok = incoming.RemoveNative(active, folder.get());
    std::cout << "source.remove=" << ok << std::endl;
    return ok ? 0 : 1;
}

#pragma once

#include <windows.h>
#include <objbase.h>
#include <cstdint>
#include <string>
#include <type_traits>

namespace quickky {

constexpr ULONG_PTR kGuardProtocol = 0x514b4750;
constexpr std::uint32_t kGuardVersion = 7;
constexpr unsigned kMaxWorkspaceTabs = 32;
constexpr unsigned kMaxNativeTabs = 128;
enum class GuardCommand : std::uint32_t {
    Attach = 1, Status = 2, Detach = 3, Configure = 4, Present = 5, Input = 6,
    Commit = 7, Close = 8, NativeSnapshot = 9, NativeAdd = 10, NativeRemove = 11,
    NativeReveal = 12
};
enum class TabInput : std::uint32_t { Select = 1, Close = 2, New = 3, Next = 4, Previous = 5 };
// Diagnostic child/clipping chrome is an explicit fixture, never a platform fallback.
enum class WorkspaceMode : std::uint32_t { Classic = 1, Native = 2, Diagnostic = 3 };

struct WireTab {
    std::uint64_t id = 0;
    wchar_t title[96]{};
};

// Pointer-free WM_COPYDATA header. Only New input appends a <=64-KiB absolute PIDL;
// size covers the entire packet. Never retain lpData past receipt.
// Nonce + controller lifetime + sender HWND bind commands to one attachment.
// Status low bits distinguish prepared (2) from committed (1); bits 8..15 are
// native-view count and bits 16+ count rejected native-view creation attempts.
struct GuardRequest {
    std::uint32_t size = sizeof(GuardRequest);
    std::uint32_t version = kGuardVersion;
    GuardCommand command = GuardCommand::Attach;
    std::uint32_t controller_pid = 0;
    std::uint64_t controller_started_at = 0;
    std::uint64_t target = 0;
    GUID nonce{};
    std::uint32_t workspace = 0;
    std::uint32_t count = 0;
    std::uint64_t active = 0;
    WireTab tabs[kMaxWorkspaceTabs]{};
    WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
    std::uint32_t visible = 1;
    TabInput input = TabInput::Select;
    std::uint64_t input_tab = 0;
};

inline std::wstring LeaseEventName(const GUID& nonce)
{
    wchar_t text[40]{};
    if (!StringFromGUID2(nonce, text, 40)) return {};
    return std::wstring(L"Local\\QTabs.Lease.") + text;
}

inline std::wstring NativeMappingName(const GUID& nonce)
{
    return LeaseEventName(nonce) + L".Native";
}

// Single outstanding request per lease. Interlocked state publishes the reply;
// a timeout retires the lease/mapping instead of racing a late target callback.
struct NativeExchange {
    volatile LONG state = 0; // 0: ready, 1: running, 2: completed
    std::uint32_t version = kGuardVersion;
    std::uint64_t request = 0;
    HRESULT result = E_PENDING;
    std::uint32_t count = 0;
    GUID active{};
    GUID created{};
    GUID tabs[kMaxNativeTabs]{};
    std::uint32_t folder_bytes = 0;
    unsigned char folder[65536]{};
};

static_assert(std::is_trivially_copyable_v<GuardRequest>, "IPC requests must be value-only.");

} // namespace quickky

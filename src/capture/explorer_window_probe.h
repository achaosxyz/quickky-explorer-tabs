#pragma once

#include "common/window_identity.h"

#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <optional>
#include <string>
#include <vector>

namespace quickky {

class ExplorerWindowProbe {
public:
    bool Initialize();
    std::optional<WindowCandidate> Inspect(HWND window, bool include_hidden = false) const;

private:
    Microsoft::WRL::ComPtr<IVirtualDesktopManager> desktops_;
    std::vector<unsigned char> user_;
    std::wstring explorer_path_;
    DWORD session_ = 0;
    DWORD integrity_ = 0;
    bool ready_ = false;
};

} // namespace quickky

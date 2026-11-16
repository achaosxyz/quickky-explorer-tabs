#pragma once

#include <windows.h>

namespace quickky {

// Native-transfer input classification used by the message hook and owned tests.
inline bool IsNativeTransferInput(UINT message, WPARAM buttons)
{
    // Windows can synthesize hover during activation/layout. Only button-held
    // motion is an action (for example a selection drag); state is also checked
    // independently before source removal.
    if (message == WM_MOUSEMOVE)
        return (buttons & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON | MK_XBUTTON1 | MK_XBUTTON2)) != 0;
    return (message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
        (message >= WM_MOUSEFIRST && message <= WM_MOUSELAST) ||
        (message >= WM_NCLBUTTONDOWN && message <= WM_NCXBUTTONDBLCLK);
}

} // namespace quickky

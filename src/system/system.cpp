#include "base/error_handling.h"

#include "system/system.h"

#include <ctime>
#include <iomanip>
#include <sstream>

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

namespace PY4GW {

namespace {

const wchar_t* kPropName = L"BL_STATE_PTR";

}  // namespace

System& System::Instance() {
    static System instance;
    return instance;
}

std::string System::GetTimestamp(const char* format) {
    auto now = std::time(nullptr);
    std::tm time_info = {};
    localtime_s(&time_info, &now);
    std::ostringstream timestamp;
    timestamp << std::put_time(&time_info, format);
    return timestamp.str();
}

/* ---------------- WindowCfg borderless internals ---------------- */

BorderlessState* WindowCfg::GetState(HWND hwnd) {
    return reinterpret_cast<BorderlessState*>(GetPropW(hwnd, kPropName));
}

void WindowCfg::SetState(HWND hwnd, BorderlessState* state) {
    if (state) {
        SetPropW(hwnd, kPropName, reinterpret_cast<HANDLE>(state));
    } else {
        RemovePropW(hwnd, kPropName);
    }
}

// Emulate resize/move on NCHITTEST while borderless.
LRESULT WindowCfg::HitTestBorderless(HWND hwnd, BorderlessState* state, LPARAM lparam) {
    if (!state || !state->draggable) {
        return HTCLIENT;
    }

    POINT pt_client{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
    ScreenToClient(hwnd, &pt_client);

    RECT rc_client{};
    GetClientRect(hwnd, &rc_client);
    const int w = rc_client.right - rc_client.left;
    const int h = rc_client.bottom - rc_client.top;

    const int x = pt_client.x;
    const int y = pt_client.y;
    const int border = (state->resize_px > 0 ? state->resize_px : 8);

    // If maximized, don't offer resize edges
    if (IsZoomed(hwnd)) {
        return HTCAPTION;  // drag anywhere
    }

    const bool left = (x >= 0 && x < border);
    const bool right = (x <= w && x > w - border);
    const bool top = (y >= 0 && y < border);
    const bool bottom = (y <= h && y > h - border);

    if (top && left) return HTTOPLEFT;
    if (top && right) return HTTOPRIGHT;
    if (bottom && left) return HTBOTTOMLEFT;
    if (bottom && right) return HTBOTTOMRIGHT;
    if (top) return HTTOP;
    if (bottom) return HTBOTTOM;
    if (left) return HTLEFT;
    if (right) return HTRIGHT;

    return HTCAPTION;  // drag everywhere else
}

LRESULT CALLBACK WindowCfg::BorderlessProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    BorderlessState* state = GetState(hwnd);

    // If state missing or disabled, pass to original immediately
    if (!state || !state->enabled) {
        WNDPROC orig = state ? state->orig_proc : reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_WNDPROC));
        return CallWindowProc(orig, hwnd, msg, wparam, lparam);
    }

    switch (msg) {
    case WM_NCCALCSIZE:
        // Tell Windows the whole window is client area (no title/borders).
        return 0;

    case WM_NCPAINT:
        // Suppress default non-client painting (title bar, borders).
        return 0;

    case WM_NCACTIVATE:
        // Prevent Windows from drawing inactive/active titlebar transitions.
        return TRUE;

    case WM_STYLECHANGING:
        // Keep borderless look by stripping frame bits if the app tries to add them.
        if (wparam == GWL_STYLE) {
            STYLESTRUCT* ss = reinterpret_cast<STYLESTRUCT*>(lparam);
            ss->styleNew &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU);
        }
        break;

    case WM_NCHITTEST:
        return HitTestBorderless(hwnd, state, lparam);

    case WM_DESTROY:
    case WM_NCDESTROY:
        // Clean up our state when the window is going away
        if (state && state->orig_proc) {
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(state->orig_proc));
        }
        SetState(hwnd, nullptr);
        delete state;
        break;
    }
    return CallWindowProc(state->orig_proc, hwnd, msg, wparam, lparam);
}

// Enable/disable true borderless. draggable=true lets the client area drag the
// window with resize edges of resize_px. Forces a frame change so Windows
// recalculates immediately.
bool WindowCfg::EnableTrueBorderless(HWND hwnd, bool enable, bool draggable, int resize_px) {
    if (!IsWindow(hwnd)) {
        return false;
    }

    BorderlessState* state = GetState(hwnd);

    if (enable) {
        if (!state) {
            state = new BorderlessState();
            state->orig_proc = reinterpret_cast<WNDPROC>(GetWindowLongPtr(hwnd, GWLP_WNDPROC));
            SetState(hwnd, state);
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(BorderlessProc));
        }
        state->enabled = TRUE;
        state->draggable = draggable;
        state->resize_px = resize_px > 0 ? resize_px : 8;
    } else if (state) {
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(state->orig_proc));
        SetState(hwnd, nullptr);
        delete state;
    }

    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER);
    return true;
}

}  // namespace PY4GW

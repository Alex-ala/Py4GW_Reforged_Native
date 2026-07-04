#include "base/error_handling.h"

#include "overlay/screen_overlay.h"

// GDI+ prerequisites: the build defines WIN32_LEAN_AND_MEAN and NOMINMAX, so
// IStream/PROPID must come from objidl.h and Gdiplus needs min/max shims.
#include <objidl.h>
#include <algorithm>
namespace Gdiplus {
using std::max;
using std::min;
}
#include <gdiplus.h>
#include <commctrl.h>   // SetWindowSubclass, DefSubclassProc

#ifndef WM_OVERLAY_HEARTBEAT
#define WM_OVERLAY_HEARTBEAT (WM_APP + 1)
#endif

namespace PY4GW::overlay {

namespace {

POINT virtual_origin_{ 0, 0 };

ATOM RegisterOverlayWnd() {
    WNDCLASSW wc{};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = L"DesktopOverlayBox";
    return RegisterClassW(&wc);
}

}  // namespace

bool ScreenOverlay::CreatePrimary(int ms, bool destroy) {
    if (!gdip_token_) {
        Gdiplus::GdiplusStartupInput gi;
        if (Gdiplus::GdiplusStartup(&gdip_token_, &gi, nullptr) != Gdiplus::Ok) return false;
    }
    static ATOM cls = RegisterOverlayWnd(); (void)cls;

    // --- use virtual desktop metrics (all monitors) ---
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    virtual_origin_.x = vx;
    virtual_origin_.y = vy;

    DWORD ex = WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
    hwnd_ = CreateWindowExW(ex, L"DesktopOverlayBox", L"", WS_POPUP,
        vx, vy, vw, vh,    // position at virtual origin, size to full span
        nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!hwnd_) return false;

    bf_.BlendOp = AC_SRC_OVER;
    bf_.BlendFlags = 0;
    bf_.AlphaFormat = AC_SRC_ALPHA;
    bf_.SourceConstantAlpha = 255;

    memdc_ = CreateCompatibleDC(nullptr);
    if (!memdc_) return false;

    if (!ensureBitmap(vw, vh)) return false;   // bitmap matches virtual size

    // Subclass THIS window only; refdata = this
    SetWindowSubclass(hwnd_, &ScreenOverlay::SubclassProc, 0xBEEF, reinterpret_cast<DWORD_PTR>(this));

    last_present_ms_ = GetTickCount64();

    SetWindowPos(hwnd_, HWND_TOPMOST, vx, vy, vw, vh, SWP_SHOWWINDOW | SWP_NOACTIVATE);
    ShowWindow(hwnd_, SW_SHOWNA);

    SetAutoExpire(ms, destroy);
    return true;
}

bool ScreenOverlay::ensureBitmap(int w, int h) {
    if (size_.cx == w && size_.cy == h && dib_) return true;
    if (dib_) { DeleteObject(dib_); dib_ = nullptr; dib_bits_ = nullptr; }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    dib_ = CreateDIBSection(memdc_, &bmi, DIB_RGB_COLORS, &dib_bits_, nullptr, 0);
    if (!dib_) return false;

    SelectObject(memdc_, dib_);
    size_.cx = w; size_.cy = h;
    return true;
}

void ScreenOverlay::Destroy() {
    disarmTimer();
    if (hwnd_) {
        RemoveWindowSubclass(hwnd_, &ScreenOverlay::SubclassProc, 0xBEEF);
    }
    if (dib_) { DeleteObject(dib_); dib_ = nullptr; dib_bits_ = nullptr; }
    if (memdc_) { DeleteDC(memdc_); memdc_ = nullptr; }
    if (hwnd_) { DestroyWindow(hwnd_); hwnd_ = nullptr; }
    if (gdip_token_) { Gdiplus::GdiplusShutdown(gdip_token_); gdip_token_ = 0; }
}

void ScreenOverlay::Show(bool show) {
    if (!hwnd_) return;
    ShowWindow(hwnd_, show ? SW_SHOWNA : SW_HIDE);
}

void ScreenOverlay::Begin() {
    if (dib_bits_) memset(dib_bits_, 0, size_.cx * size_.cy * 4); // fully transparent
}

std::tuple<int, int> ScreenOverlay::GetDesktopSize() {
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    return std::make_tuple(w, h);
}

void ScreenOverlay::DrawRect(int x, int y, int w, int h, unsigned int argb, float thickness) {
    if (!memdc_) return;
    Gdiplus::Graphics g(memdc_);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(Gdiplus::Color(argb), thickness);
    g.DrawRectangle(&pen, x, y, w, h);
}

void ScreenOverlay::DrawRectFilled(int x, int y, int w, int h, unsigned int argb) {
    if (!memdc_) return;
    Gdiplus::Graphics g(memdc_);
    g.SetSmoothingMode(Gdiplus::SmoothingModeNone);
    Gdiplus::SolidBrush brush(Gdiplus::Color((Gdiplus::ARGB)argb));
    Gdiplus::Rect rc((INT)x, (INT)y, (INT)w, (INT)h);
    g.FillRectangle(&brush, rc);
}

void ScreenOverlay::DrawTextBox(int x, int y, int w, int h,
    const std::wstring& text,
    unsigned int argb,
    float px_size,
    const wchar_t* family,
    bool hcenter, bool vcenter) {
    if (!memdc_) return;
    Gdiplus::Graphics g(memdc_);
    g.SetTextRenderingHint(Gdiplus::TextRenderingHintClearTypeGridFit);
    Gdiplus::SolidBrush brush(Gdiplus::Color((Gdiplus::ARGB)argb));
    Gdiplus::Font font(family, px_size, Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

    Gdiplus::RectF rc((Gdiplus::REAL)x, (Gdiplus::REAL)y, (Gdiplus::REAL)w, (Gdiplus::REAL)h);
    Gdiplus::StringFormat fmt;
    fmt.SetTrimming(Gdiplus::StringTrimmingEllipsisCharacter);
    fmt.SetFormatFlags(Gdiplus::StringFormatFlagsLineLimit);
    if (hcenter) fmt.SetAlignment(Gdiplus::StringAlignmentCenter);
    if (vcenter) fmt.SetLineAlignment(Gdiplus::StringAlignmentCenter);

    g.DrawString(text.c_str(), -1, &font, rc, &fmt, &brush);
}

bool ScreenOverlay::present() {
    if (!hwnd_ || !dib_) return false;
    HDC screen = GetDC(nullptr);
    POINT src = { 0, 0 };
    POINT dst = virtual_origin_;          // must be virtual origin, not {0,0}
    SIZE  sz = size_;
    BOOL ok = UpdateLayeredWindow(hwnd_, screen, &dst, &sz, memdc_, &src, 0, &bf_, ULW_ALPHA);
    ReleaseDC(nullptr, screen);
    return ok != FALSE;
}

void ScreenOverlay::End() {
    if (present()) {
        last_present_ms_ = GetTickCount64();
        if (expire_ms_) armTimer();  // re-arm one-shot to expire after idle period
    }
}

void ScreenOverlay::SetAutoExpire(int ms, bool destroy) {
    expire_ms_ = static_cast<DWORD>(ms);
    expire_destroy_ = destroy;
    if (expire_ms_) {
        if (!timer_) armTimer();
    }
    else {
        disarmTimer();
    }
}

VOID CALLBACK ScreenOverlay::TimerThunk(PVOID param, BOOLEAN) {
    auto* self = static_cast<ScreenOverlay*>(param);
    if (!self || !self->hwnd_) return;
    // wParam = 1 means "expire now"
    PostMessageW(self->hwnd_, WM_OVERLAY_HEARTBEAT, 1, 0);
}

void ScreenOverlay::armTimer() {
    if (!expire_ms_) return;

    // cancel any previous timer so we can re-arm with a fresh due time
    if (timer_) {
        HANDLE t = timer_;
        timer_ = nullptr;
        DeleteTimerQueueTimer(timer_queue_, t, INVALID_HANDLE_VALUE);
    }

    if (!timer_queue_) {
        timer_queue_ = CreateTimerQueue();
        if (!timer_queue_) return;
    }

    // one-shot: dueTime = expire_ms_, period = 0
    CreateTimerQueueTimer(
        &timer_,
        timer_queue_,
        &ScreenOverlay::TimerThunk,
        this,
        expire_ms_,
        0,            // one-shot
        WT_EXECUTEDEFAULT
    );
}

void ScreenOverlay::disarmTimer() {
    if (timer_) {
        HANDLE t = timer_;
        timer_ = nullptr;
        DeleteTimerQueueTimer(timer_queue_, t, INVALID_HANDLE_VALUE); // wait for callbacks to finish
    }
    if (timer_queue_) {
        HANDLE q = timer_queue_;
        timer_queue_ = nullptr;
        DeleteTimerQueueEx(q, INVALID_HANDLE_VALUE); // wait for all timers
    }
}

LRESULT CALLBACK ScreenOverlay::SubclassProc(HWND h, UINT m, WPARAM w, LPARAM l,
    UINT_PTR /*id*/, DWORD_PTR ref) {
    auto* self = reinterpret_cast<ScreenOverlay*>(ref);
    if (!self) return DefSubclassProc(h, m, w, l);

    if (m == WM_OVERLAY_HEARTBEAT) {
        // if wParam==1, this is the one-shot expiry; no clock check needed
        if (w == 1 || (self->expire_ms_ && GetTickCount64() - self->last_present_ms_ > self->expire_ms_)) {
            if (self->expire_destroy_) {
                PostMessageW(self->hwnd_, WM_CLOSE, 0, 0);
            }
            else {
                self->Show(false);
            }
            self->disarmTimer();
        }
        return 0;
    }

    // Handle WM_CLOSE if we posted it for destroy-expire
    if (m == WM_CLOSE) {
        // Let Destroy() do the heavy lifting
        self->Destroy();
        return 0;
    }

    return DefSubclassProc(h, m, w, l);
}

}  // namespace PY4GW::overlay

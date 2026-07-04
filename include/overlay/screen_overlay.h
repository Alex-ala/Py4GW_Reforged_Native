#pragma once

#include "base/error_handling.h"

#include <string>
#include <tuple>

#include <windows.h>

// Desktop-level overlay migrated from legacy py_overlay.h/.cpp (ScreenOverlay).
// GDI+ drawing into a layered, click-through, topmost window spanning the
// virtual desktop; independent of the game window and Direct3D entirely.
namespace PY4GW::overlay {

class ScreenOverlay {
public:
    ~ScreenOverlay() { Destroy(); }
    bool CreatePrimary(int expire_ms = 500, bool destroy = false); // primary monitor
    void Destroy();
    void Show(bool show);

    void Begin();                           // clear frame (transparent)
    void DrawRect(int x, int y, int w, int h, unsigned int argb, float thickness = 1.0f);
    void DrawRectFilled(int x, int y, int w, int h, unsigned int argb);
    void DrawTextBox(int x, int y, int w, int h,
        const std::wstring& text,
        unsigned int argb,
        float px_size = 16.0f,
        const wchar_t* family = L"Segoe UI",
        bool hcenter = false, bool vcenter = false);
    void End();                             // present with UpdateLayeredWindow

    // Auto-expire after 'ms' without successful End()/present.
    // ms == 0 disables the watchdog. destroy=true -> Destroy(), else Show(false).
    void SetAutoExpire(int ms, bool destroy = false);
    std::tuple<int, int> GetDesktopSize();

private:
    bool ensureBitmap(int w, int h);
    bool present();
    void armTimer();
    void disarmTimer();

    // Subclass proc to handle heartbeat.
    static LRESULT CALLBACK SubclassProc(HWND h, UINT m, WPARAM w, LPARAM l,
        UINT_PTR id, DWORD_PTR ref);

    // Timer queue callback (no UI work here).
    static VOID CALLBACK TimerThunk(PVOID param, BOOLEAN /*timer_or_wait*/);

private:
    HWND   hwnd_ = nullptr;
    HDC    memdc_ = nullptr;
    HBITMAP dib_ = nullptr;
    void* dib_bits_ = nullptr;
    SIZE   size_{};
    BLENDFUNCTION bf_{};
    ULONG_PTR gdip_token_ = 0;

    // --- state for auto-expire ---
    HANDLE  timer_queue_ = nullptr;
    HANDLE  timer_ = nullptr;
    DWORD   expire_ms_ = 0;          // 0 = disabled
    bool    expire_destroy_ = false;
    ULONGLONG last_present_ms_ = 0;  // updated on successful present()
};

}  // namespace PY4GW::overlay

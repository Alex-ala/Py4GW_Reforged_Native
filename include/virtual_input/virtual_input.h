#pragma once

#include "base/error_handling.h"

#include <vector>

#include <windows.h>

namespace PY4GW {

// Parity port of the legacy VirtualInput.h (KeyHandler / MouseHandler).
// Input is posted directly to the Guild Wars window. Deviation from legacy:
// the target window comes from MemoryManager::GetGWWindowHandle() instead of
// the legacy gw_client_window_handle global, which does not exist here.

class KeyHandler {
    HWND targetWindow;

public:
    KeyHandler();

    void set_target_window(HWND windowHandle);
    HWND get_target_window() const;

    bool is_extended_key(int vk);
    void send_key(int virtualKeyCode, bool isKeyUp = false);

    // Press a single key
    void press_key(int virtualKeyCode);
    // Release a single key
    void release_key(int virtualKeyCode);
    // Press and release a key (push key)
    void push_key(int virtualKeyCode);

    // Press a combination of keys
    void press_key_combo(const std::vector<int>& keys);
    // Release a combination of keys
    void release_key_combo(const std::vector<int>& keys);
    // Push a combination of keys
    void push_key_combo(const std::vector<int>& keys);
};

class MouseHandler {
    HWND targetWindow;

public:
    MouseHandler();

    void set_target_window(HWND windowHandle);
    HWND get_target_window() const;

    // Move mouse virtually within client area (no real cursor movement)
    void MoveMouse(int x, int y);
    // Click (Left=0, Right=1, Middle=2)
    void Click(int button = 0, int x = 0, int y = 0);
    // Double Click
    void DoubleClick(int button = 0, int x = 0, int y = 0);
    // Press button down
    void PressButton(int button = 0, int x = 0, int y = 0);
    // Release button
    void ReleaseButton(int button = 0, int x = 0, int y = 0);
    // Scroll (positive = up, negative = down)
    void Scroll(int delta, int x = 0, int y = 0);
};

}  // namespace PY4GW

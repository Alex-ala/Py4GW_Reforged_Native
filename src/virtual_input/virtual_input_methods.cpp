#include "base/error_handling.h"

#include "virtual_input/virtual_input.h"

#include "base/memory_manager.h"

namespace PY4GW {

/* ---------------- KeyHandler ---------------- */

KeyHandler::KeyHandler() {
    targetWindow = MemoryManager::GetGWWindowHandle();
}

void KeyHandler::set_target_window(HWND windowHandle) {
    targetWindow = windowHandle;
}

HWND KeyHandler::get_target_window() const {
    return targetWindow;
}

bool KeyHandler::is_extended_key(int vk) {
    switch (vk) {
    case VK_RIGHT:
    case VK_LEFT:
    case VK_UP:
    case VK_DOWN:
    case VK_INSERT:
    case VK_DELETE:
    case VK_HOME:
    case VK_END:
    case VK_PRIOR:  // Page Up
    case VK_NEXT:   // Page Down
    case VK_NUMLOCK:
    case VK_DIVIDE: // Numpad Divide
    case VK_RETURN: // Only if it's the numpad Enter
        return true;
    default:
        return false;
    }
}

void KeyHandler::send_key(int virtualKeyCode, bool isKeyUp) {
    if (!targetWindow) return;

    // Base lParam setup
    LPARAM lParam = 1; // Repeat count = 1
    lParam |= MapVirtualKey(virtualKeyCode, MAPVK_VK_TO_VSC) << 16;

    // Check if it's an extended key
    if (is_extended_key(virtualKeyCode)) {
        lParam |= 0x01000000;  // Set Extended Key flag
    }

    if (isKeyUp) {
        lParam |= 0xC0000000;  // Key-up flag and previous key state
    }

    SendMessage(targetWindow, isKeyUp ? WM_KEYUP : WM_KEYDOWN, virtualKeyCode, lParam);
}

void KeyHandler::press_key(int virtualKeyCode) {
    send_key(virtualKeyCode, false);
}

void KeyHandler::release_key(int virtualKeyCode) {
    send_key(virtualKeyCode, true);
}

void KeyHandler::push_key(int virtualKeyCode) {
    press_key(virtualKeyCode);
    Sleep(100); // Small delay to mimic key press duration
    release_key(virtualKeyCode);
}

void KeyHandler::press_key_combo(const std::vector<int>& keys) {
    for (int key : keys) {
        press_key(key);
    }
}

void KeyHandler::release_key_combo(const std::vector<int>& keys) {
    for (int key : keys) {
        release_key(key);
    }
}

void KeyHandler::push_key_combo(const std::vector<int>& keys) {
    press_key_combo(keys);
    Sleep(100); // Mimic a real delay
    release_key_combo(keys);
}

/* ---------------- MouseHandler ---------------- */

MouseHandler::MouseHandler() {
    targetWindow = MemoryManager::GetGWWindowHandle();
}

void MouseHandler::set_target_window(HWND windowHandle) {
    targetWindow = windowHandle;
}

HWND MouseHandler::get_target_window() const {
    return targetWindow;
}

void MouseHandler::MoveMouse(int x, int y) {
    if (!targetWindow) return;

    LPARAM lParam = (y << 16) | (x & 0xFFFF);
    PostMessage(targetWindow, WM_MOUSEMOVE, 0, lParam);
}

void MouseHandler::Click(int button, int x, int y) {
    if (!targetWindow) return;

    PressButton(button, x, y);
    ReleaseButton(button, x, y);
}

void MouseHandler::DoubleClick(int button, int x, int y) {
    if (!targetWindow) return;

    LPARAM lParam = (y << 16) | (x & 0xFFFF);
    UINT msg;

    if (button == 2)
        msg = WM_MBUTTONDBLCLK;
    else if (button == 1)
        msg = WM_RBUTTONDBLCLK;
    else
        msg = WM_LBUTTONDBLCLK;

    PostMessage(targetWindow, msg, 0, lParam);
}

void MouseHandler::PressButton(int button, int x, int y) {
    if (!targetWindow) return;

    LPARAM lParam = (y << 16) | (x & 0xFFFF);
    UINT msg;

    if (button == 2)
        msg = WM_MBUTTONDOWN;
    else if (button == 1)
        msg = WM_RBUTTONDOWN;
    else
        msg = WM_LBUTTONDOWN;

    PostMessage(targetWindow, msg, 0, lParam);
}

void MouseHandler::ReleaseButton(int button, int x, int y) {
    if (!targetWindow) return;

    LPARAM lParam = (y << 16) | (x & 0xFFFF);
    UINT msg;

    if (button == 2)
        msg = WM_MBUTTONUP;
    else if (button == 1)
        msg = WM_RBUTTONUP;
    else
        msg = WM_LBUTTONUP;

    PostMessage(targetWindow, msg, 0, lParam);
}

void MouseHandler::Scroll(int delta, int x, int y) {
    if (!targetWindow) return;

    LPARAM lParam = (y << 16) | (x & 0xFFFF);
    WPARAM wParam = (delta << 16);  // Scroll amount in high word

    PostMessage(targetWindow, WM_MOUSEWHEEL, wParam, lParam);
}

}  // namespace PY4GW

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <windows.h>
#include <vector>
#include "Keyboard.h"
#include "Utils/Network.h"
#include "Components/MessageInput.h"

extern HWND hInputEdit;

bool HandleKeyboardInput(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, HWND hInputEdit) {
    if (GetFocus() != hInputEdit) return false;
    
    if (uMsg == WM_KEYDOWN) {
        bool ctrl = GetKeyState(VK_CONTROL) < 0;
        bool shift = GetKeyState(VK_SHIFT) < 0;
        
        if (ctrl) {
            switch (wParam) {
            case 'A': case 'a':
                SendMessage(hInputEdit, EM_SETSEL, 0, -1);
                return true;
            case 'C': case 'c':
                SendMessage(hInputEdit, WM_COPY, 0, 0);
                return true;
            case 'V': case 'v':
                SendMessage(hInputEdit, WM_PASTE, 0, 0);
                return true;
            case 'X': case 'x':
                SendMessage(hInputEdit, WM_CUT, 0, 0);
                return true;
            case 'Z': case 'z':
                SendMessage(hInputEdit, EM_UNDO, 0, 0);
                return true;
            }
        }
        
        if (wParam == VK_RETURN && !shift) {
            SendChatMessage();
            return true;
        }
    }
    
    if (uMsg == WM_CHAR) {
        if (wParam == VK_RETURN) {
            if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
                return true;
            }
        }
    }
    
    return false;
}

void ProcessMessageLoop() {
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.hwnd == hInputEdit) {
            if (msg.wParam == VK_RETURN) {
                if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
                    SendChatMessage();
                    continue;
                }
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "Keyboard.h"
#include "Utils/Network.h"
#include "Components/MessageInput.h"

extern HWND hInputEdit;

int HandleKeyboardInput(void* hwnd, unsigned int uMsg, unsigned long long wParam, long long lParam, void* hInputEdit_param) {
    HWND hInput = (HWND)hInputEdit_param;
    if (GetFocus() != hInput) return 0;
    
    if (uMsg == WM_KEYDOWN) {
        int ctrl = GetKeyState(VK_CONTROL) < 0;
        int shift = GetKeyState(VK_SHIFT) < 0;
        
        if (ctrl) {
            switch (wParam) {
            case 'A': case 'a':
                SendMessage(hInput, EM_SETSEL, 0, -1);
                return 1;
            case 'C': case 'c':
                SendMessage(hInput, WM_COPY, 0, 0);
                return 1;
            case 'V': case 'v':
                SendMessage(hInput, WM_PASTE, 0, 0);
                return 1;
            case 'X': case 'x':
                SendMessage(hInput, WM_CUT, 0, 0);
                return 1;
            case 'Z': case 'z':
                SendMessage(hInput, EM_UNDO, 0, 0);
                return 1;
            }
        }
        
        if (wParam == VK_RETURN && !shift) {
            SendPrivateMessageFromUI();
            return 1;
        }
    }
    
    if (uMsg == WM_CHAR) {
        if (wParam == VK_RETURN) {
            if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
                return 1;
            }
        }
    }
    
    return 0;
}

void ProcessMessageLoop(void) {
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (msg.message == WM_KEYDOWN && msg.hwnd == hInputEdit) {
            if (msg.wParam == VK_RETURN) {
                if (!(GetKeyState(VK_SHIFT) & 0x8000)) {
                    SendPrivateMessageFromUI();
                    continue;
                }
            }
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}
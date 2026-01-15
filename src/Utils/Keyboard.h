#ifndef UTILS_KEYBOARD_H
#define UTILS_KEYBOARD_H

#include <windows.h>

using HWNDHandle = HWND;
using MsgCode    = UINT;
using WParam     = WPARAM;
using LParam     = LPARAM;

int HandleKeyboardInput(
    HWNDHandle hwnd,
    MsgCode msg,
    WParam wParam,
    LParam lParam,
    void* userData
);

void ProcessMessageLoop();

#endif // UTILS_KEYBOARD_H

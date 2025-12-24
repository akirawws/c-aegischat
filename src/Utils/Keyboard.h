#ifndef KEYBOARD_H
#define KEYBOARD_H

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

bool HandleKeyboardInput(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam, HWND hInputEdit);
void ProcessMessageLoop();

#endif


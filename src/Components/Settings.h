#pragma once
#include <windows.h>
#include <string>

void OpenSettingsDialog(HWND parent);

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
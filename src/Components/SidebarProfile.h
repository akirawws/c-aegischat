#pragma once
#include <windows.h>
#include <gdiplus.h>
#include <string>

const int PROFILE_PANEL_HEIGHT = 52;

void DrawSidebarProfile(Gdiplus::Graphics& g, int x, int windowHeight, int totalWidth, const std::string& fallbackName);
bool IsClickOnProfile(int x, int y, int sidebarX, int windowHeight, int width);
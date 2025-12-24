#ifndef FRIENDSPAGE_H
#define FRIENDSPAGE_H

#include <windows.h>
#include <string>
#include <vector>

// Состояния фильтрации
enum class FriendsFilter { All, Online, Pending };

void DrawFriendsPage(HDC hdc, HWND hwnd, int width, int height);
void HandleFriendsClick(HWND hwnd, int x, int y);

#endif
#pragma once
#include <windows.h>

// Глобальная переменная для высоты контента (нужна для ограничения скролла)
extern int g_totalBlogHeight; 

// Функция отрисовки принимает текущий оффсет скролла
void DrawDeveloperBlogPage(HDC memDC, RECT rect, int startX, int scrollOffset);
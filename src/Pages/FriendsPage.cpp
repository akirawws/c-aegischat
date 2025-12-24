#include "FriendsPage.h"
#include "Utils/Styles.h"
#include <string>

FriendsFilter currentFilter = FriendsFilter::Online;

void DrawFriendsPage(HDC hdc, HWND hwnd, int width, int height) {
    int sidebarWidth = 72;
    int contentX = sidebarWidth;
    int contentWidth = width - sidebarWidth;

    // 1. Фон основной области
    RECT bgRect = { contentX, 0, width, height };
    HBRUSH hBgBrush = CreateSolidBrush(COLOR_BG_DARK);
    FillRect(hdc, &bgRect, hBgBrush);
    DeleteObject(hBgBrush);

    // 2. Верхняя панель (Header)
    RECT headerRect = { contentX, 0, width, 48 };
    HBRUSH hHeaderBrush = CreateSolidBrush(RGB(49, 51, 56)); 
    FillRect(hdc, &headerRect, hHeaderBrush);
    
    // Линия под хедером
    HPEN hPen = CreatePen(PS_SOLID, 1, RGB(30, 31, 34));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, contentX, 48, NULL);
    LineTo(hdc, width, 48);

    // 3. Текст и кнопки фильтров (Используем W-версии)
    SetBkMode(hdc, TRANSPARENT);
    HFONT hFontBold = CreateAppFont(14, FONT_WEIGHT_BOLD);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFontBold);
    
    // Заголовок "Друзья"
    SetTextColor(hdc, RGB(255, 255, 255));
    RECT textFriends = { contentX + 15, 0, contentX + 100, 48 };
    DrawTextW(hdc, L"Друзья", -1, &textFriends, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    // Вертикальный разделитель между "Друзья" и кнопками
    MoveToEx(hdc, contentX + 85, 15, NULL);
    LineTo(hdc, contentX + 85, 33);

    // Кнопки фильтров
    const wchar_t* filters[] = { L"В сети", L"Все", L"Ожидание" };
    int startX = contentX + 100;
    
    for(int i = 0; i < 3; i++) {
        bool isActive = ((int)currentFilter == i);
        SetTextColor(hdc, isActive ? RGB(255, 255, 255) : RGB(181, 186, 193));
        
        RECT r = { startX, 10, startX + 80, 38 };
        if (isActive) {
            HBRUSH hActive = CreateSolidBrush(RGB(67, 71, 78));
            FillRect(hdc, &r, hActive);
            DeleteObject(hActive);
        }
        
        DrawTextW(hdc, filters[i], -1, &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        startX += 90;
    }

    // 4. Кнопка "Добавить в друзья" (Зеленая в стиле Discord)
    RECT btnAdd = { width - 170, 10, width - 20, 38 };
    HBRUSH hBtnBrush = CreateSolidBrush(RGB(36, 128, 70)); 
    SelectObject(hdc, hBtnBrush);
    RoundRect(hdc, btnAdd.left, btnAdd.top, btnAdd.right, btnAdd.bottom, 5, 5);
    
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, L"Добавить в друзья", -1, &btnAdd, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    // 5. Текст заглушки в центре
    SetTextColor(hdc, RGB(148, 155, 164));
    RECT emptyRect = { contentX, 48, width, height };
    DrawTextW(hdc, L"Никто не хочет с тобой общаться... пока что.", -1, &emptyRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Очистка
    SelectObject(hdc, hOldFont);
    SelectObject(hdc, hOldPen);
    DeleteObject(hFontBold);
    DeleteObject(hHeaderBrush);
    DeleteObject(hBtnBrush);
    DeleteObject(hPen);


}
void HandleFriendsClick(HWND hwnd, int x, int y) {
    int sidebarWidth = 72;
    int startX = sidebarWidth + 100;

    // 1. Логика переключения фильтров (В сети | Все | Ожидание)
    for (int i = 0; i < 3; i++) {
        RECT r = { startX, 10, startX + 80, 38 };
        if (x >= r.left && x <= r.right && y >= r.top && y <= r.bottom) {
            currentFilter = (FriendsFilter)i;
            InvalidateRect(hwnd, NULL, TRUE); // Перерисовать страницу
            return;
        }
        startX += 90;
    }

    // 2. Логика кнопки "Добавить в друзья"
    RECT rect;
    GetClientRect(hwnd, &rect);
    RECT btnAdd = { rect.right - 170, 10, rect.right - 20, 38 };
    
    if (x >= btnAdd.left && x <= btnAdd.right && y >= btnAdd.top && y <= btnAdd.bottom) {
        MessageBoxW(hwnd, L"Введите никнейм пользователя", L"Добавить в друзья", MB_OK | MB_ICONINFORMATION);
    }
}
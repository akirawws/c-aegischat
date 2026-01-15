#include "Settings.h"
#include "../Utils/UIState.h"
#include "../Utils/Utils.h"
#include <windowsx.h>
#include <commctrl.h>
#include <gdiplus.h>
#include "Utils/Network.h"


using namespace Gdiplus;

// Глобальные переменные
static HWND g_hSettingsWnd = NULL;
static HFONT g_hFont = NULL;
static HFONT g_hBoldFont = NULL;
static int g_currentTab = 0; 
static HWND g_hProfileEdit = NULL;
static HWND g_hProfileSaveBtn = NULL;
static HWND g_hLogoutBtn = NULL;
static HWND g_hBioEdit = NULL;


// Размеры и константы
const int SETTINGS_WIDTH = 700;
const int SETTINGS_HEIGHT = 500;
const int SIDEBAR_WIDTH = 180;
const int PADDING = 24;
const int LABEL_HEIGHT = 24;
const int EDIT_HEIGHT = 32;
const int BUTTON_HEIGHT = 40;

// Цвета
#define CLR_BACKGROUND RGB(32, 34, 37)
#define CLR_SIDEBAR RGB(49, 51, 56)
#define CLR_TEXT RGB(220, 221, 222)
#define CLR_TEXT_MUTED RGB(148, 155, 164)
#define CLR_HOVER RGB(79, 84, 92)
#define CLR_SEPARATOR RGB(64, 68, 75)

const wchar_t* TAB_NAMES[] = { L"Профиль", L"Учётная запись", L"Выход" };

void ChooseAndUploadAvatar(HWND hwnd) {
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = L"Изображения\0*.png;*.jpg;*.jpeg\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        SendAvatarUpdate(ofn.lpstrFile);
        MessageBoxW(hwnd, L"Аватар отправлен на сервер!", L"AEGIS", MB_OK | MB_ICONINFORMATION);
    }
}

void UpdateControlVisibility() {
    if (!g_hSettingsWnd) return;
    bool isProfile = (g_currentTab == 0);
    ShowWindow(g_hProfileEdit, isProfile ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hBioEdit, isProfile ? SW_SHOW : SW_HIDE); // Показываем Bio
    ShowWindow(g_hProfileSaveBtn, isProfile ? SW_SHOW : SW_HIDE);
    ShowWindow(g_hLogoutBtn, (g_currentTab == 2) ? SW_SHOW : SW_HIDE);
    InvalidateRect(g_hSettingsWnd, NULL, TRUE);
}

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            HWND hToolTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL,
                WS_POPUP | TTS_ALWAYSTIP,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                hwnd, NULL, GetModuleHandle(NULL), NULL);

            TOOLINFOW ti = { 0 };
            ti.cbSize = sizeof(ti);
            ti.uFlags = TTF_SUBCLASS;
            ti.hwnd = hwnd;
            ti.lpszText = (LPWSTR)L"Нажмите, чтобы изменить аватар";
            
            // Координаты аватара в правой части
            int avatarX = SIDEBAR_WIDTH + PADDING * 2;
            int avatarY = PADDING * 2 + 50;
            ti.rect.left = avatarX;
            ti.rect.top = avatarY;
            ti.rect.right = avatarX + 80;
            ti.rect.bottom = avatarY + 80;

            SendMessageW(hToolTip, TTM_ADDTOOLW, 0, (LPARAM)&ti);

            g_hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            g_hBoldFont = CreateFontW(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
            return 0;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rc; GetClientRect(hwnd, &rc);
            HBRUSH hBg = CreateSolidBrush(CLR_BACKGROUND);
            FillRect(hdc, &rc, hBg);
            DeleteObject(hBg);

            RECT sb = {0, 0, SIDEBAR_WIDTH, SETTINGS_HEIGHT};
            HBRUSH hSb = CreateSolidBrush(CLR_SIDEBAR);
            FillRect(hdc, &sb, hSb);
            DeleteObject(hSb);

            Graphics graphics(hdc);
            graphics.SetSmoothingMode(SmoothingModeAntiAlias);

            // 1. Отрисовка Сайдбара (текст вкладок)
            SetBkMode(hdc, TRANSPARENT);
            int yPos = PADDING;
            for (int i = 0; i < 3; i++) {
                if (i == 2) yPos += 40;
                if (g_currentTab == i) {
                    RECT itemRect = {0, yPos, SIDEBAR_WIDTH, yPos + LABEL_HEIGHT};
                    HBRUSH hHover = CreateSolidBrush(CLR_HOVER);
                    FillRect(hdc, &itemRect, hHover);
                    DeleteObject(hHover);
                }
                SelectObject(hdc, g_currentTab == i ? g_hBoldFont : g_hFont);
                SetTextColor(hdc, CLR_TEXT);
                TextOutW(hdc, PADDING, yPos, TAB_NAMES[i], (int)wcslen(TAB_NAMES[i]));
                yPos += LABEL_HEIGHT + 8;
            }

            // 2. Отрисовка Контента (правая часть)
            int contentX = SIDEBAR_WIDTH + PADDING * 2;
            int contentY = PADDING * 2;

            if (g_currentTab == 0) { // ВКЛАДКА ПРОФИЛЬ
                SelectObject(hdc, g_hBoldFont);
                SetTextColor(hdc, CLR_TEXT);
                TextOutW(hdc, contentX, contentY, L"Мой профиль", 11);

                // Аватар
                int avatarY = contentY + 50;
                Rect avatarRect(contentX, avatarY, 80, 80);
                SolidBrush circleBrush(Color(255, 66, 69, 73));
                graphics.FillEllipse(&circleBrush, avatarRect);
                
                Font font(L"Segoe UI", 8, FontStyleBold);
                SolidBrush textBrush(Color(255, 200, 200, 200));
                StringFormat sf;
                sf.SetAlignment(StringAlignmentCenter);
                sf.SetLineAlignment(StringAlignmentCenter);
                graphics.DrawString(L"EDIT", -1, &font, RectF((REAL)contentX, (REAL)avatarY, 80, 80), &sf, &textBrush);

                // Метка поля ввода
                SetTextColor(hdc, CLR_TEXT_MUTED);
                SelectObject(hdc, g_hFont);
                TextOutW(hdc, contentX, avatarY + 100, L"Отображаемое имя", 16);

                TextOutW(hdc, contentX, avatarY + 100 + EDIT_HEIGHT + 12, L"О себе", 6);
                
            }
            else if (g_currentTab == 1) { // ВКЛАДКА АККАУНТ
                SelectObject(hdc, g_hBoldFont);
                SetTextColor(hdc, CLR_TEXT);
                TextOutW(hdc, contentX, contentY, L"Учётная запись", 14);
            }
            else if (g_currentTab == 2) { // ВКЛАДКА ВЫХОД
                SelectObject(hdc, g_hBoldFont);
                SetTextColor(hdc, CLR_TEXT);
                TextOutW(hdc, contentX, contentY, L"Выход", 5);
            }

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = GET_X_LPARAM(lParam);
            int y = GET_Y_LPARAM(lParam);

            // Клик по аватару (только если вкладка 0)
            if (g_currentTab == 0) {
                int avatarX = SIDEBAR_WIDTH + PADDING * 2;
                int avatarY = PADDING * 2 + 50;
                if (x >= avatarX && x <= avatarX + 80 && y >= avatarY && y <= avatarY + 80) {
                    ChooseAndUploadAvatar(hwnd);
                    return 0;
                }
            }

            // Переключение вкладок (Сайдбар)
            if (x < SIDEBAR_WIDTH) {
                int yPos = PADDING;
                for (int i = 0; i < 3; i++) {
                    if (i == 2) yPos += 40;
                    if (y >= yPos && y <= yPos + LABEL_HEIGHT) {
                        g_currentTab = i;
                        UpdateControlVisibility();
                        break;
                    }
                    yPos += LABEL_HEIGHT + 8;
                }
            }
            return 0;
        }

    case WM_COMMAND: {
        if (LOWORD(wParam) == 1001) { // Кнопка "Сохранить" на вкладке профиля
            wchar_t wDisplayName[64];
            wchar_t wBio[256];

            // Получаем текст из полей ввода
            GetWindowTextW(g_hProfileEdit, wDisplayName, 64);
            GetWindowTextW(g_hBioEdit, wBio, 256);

            // Конвертируем WideChar в UTF-8 строки
            std::string newName = WideToUtf8(wDisplayName);
            std::string newBio = WideToUtf8(wBio);

            // ВАЖНО: Отправляем пакеты по очереди
            SendDisplayNameChange(newName); // Пакет 15
            
            // ДОБАВЬТЕ ЭТУ СТРОКУ, если её нет:
            OutputDebugStringA(("[Settings] Sending Bio: " + newBio + "\n").c_str());
            SendBioChange(newBio);          // Пакет 19

            MessageBoxW(hwnd, L"Профиль обновлен!", L"AEGIS", MB_OK | MB_ICONINFORMATION);
        }
            if (LOWORD(wParam) == 3000) DestroyWindow(hwnd);
            return 0;
        }

        case WM_DESTROY: {
            g_hSettingsWnd = NULL;
            if (g_hFont) DeleteObject(g_hFont);
            if (g_hBoldFont) DeleteObject(g_hBoldFont);
            break;
        }
        default: return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void CreateProfileControls(HWND hwnd) {
    int contentX = SIDEBAR_WIDTH + PADDING * 2;
    int contentY = PADDING * 2 + 50 + 80 + 45; 
    int inputWidth = SETTINGS_WIDTH - SIDEBAR_WIDTH - PADDING * 4;

    // Поле имени (уже есть)
    g_hProfileEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", Utf8ToWide(g_uiState.userDisplayName).c_str(),
        WS_CHILD | ES_AUTOHSCROLL,
        contentX, contentY, inputWidth, EDIT_HEIGHT,
        hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessageW(g_hProfileEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);

    // НОВОЕ: Поле BIO (Многострочное)
    int bioY = contentY + EDIT_HEIGHT + 35; // Отступ вниз от имени
    g_hBioEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"Edit", L"", 
        WS_CHILD | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        contentX, bioY, inputWidth, 80, // Высота 80px
        hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessageW(g_hBioEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);
    // Подсказка для Bio (через SendMessage)
    SendMessageW(g_hBioEdit, EM_SETCUEBANNER, FALSE, (LPARAM)L"Расскажите о себе...");

    // Кнопка сохранить (сдвигаем ниже под Bio)
    g_hProfileSaveBtn = CreateWindowExW(0, L"Button", L"Сохранить изменения",
        WS_CHILD | BS_PUSHBUTTON,
        contentX, bioY + 80 + 15, 200, BUTTON_HEIGHT,
        hwnd, (HMENU)1001, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    SendMessageW(g_hProfileSaveBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);
}

void CreateLogoutControls(HWND hwnd) {
    int contentX = SIDEBAR_WIDTH + PADDING * 2;
    int contentY = PADDING * 2 + 60;

    g_hLogoutBtn = CreateWindowExW(0, L"Button", L"Выйти из аккаунта",
        WS_CHILD | BS_PUSHBUTTON,
        contentX, contentY, 200, BUTTON_HEIGHT,
        hwnd, (HMENU)3000, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
    
    SendMessageW(g_hLogoutBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);
}

void OpenSettingsDialog(HWND parent) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc = {0};
        wc.cbSize = sizeof(WNDCLASSEXW);
        wc.lpfnWndProc = SettingsWndProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hbrBackground = CreateSolidBrush(CLR_BACKGROUND);
        wc.lpszClassName = L"AegisSettingsClass";
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassExW(&wc);
        registered = true;
    }

    g_hSettingsWnd = CreateWindowExW(WS_EX_TOPMOST, L"AegisSettingsClass", L"Настройки — AEGIS",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, SETTINGS_WIDTH, SETTINGS_HEIGHT,
        parent, NULL, GetModuleHandle(NULL), NULL);

    if (g_hSettingsWnd) {
        CreateProfileControls(g_hSettingsWnd);
        CreateLogoutControls(g_hSettingsWnd);
        g_currentTab = 0;
        UpdateControlVisibility();
        ShowWindow(g_hSettingsWnd, SW_SHOW);
    }
}
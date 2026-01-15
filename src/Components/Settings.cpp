#include "Settings.h"
#include "../Utils/UIState.h"
#include "../Utils/Utils.h"
#include <windowsx.h>
#include <commctrl.h>
#include <gdiplus.h>
#include "Utils/Messages.h"
#include "Utils/Network.h"

using namespace Gdiplus;

// Глобальные переменные для окна настроек
static HWND g_hSettingsWnd = NULL;
static HWND g_hDisplayNameEdit = NULL;
static HFONT g_hFont = NULL;

// Размеры и отступы (в стиле Discord)
const int SETTINGS_WIDTH = 500;
const int SETTINGS_HEIGHT = 400;
const int PADDING = 24;
const int LABEL_HEIGHT = 24;
const int EDIT_HEIGHT = 32;
const int BUTTON_HEIGHT = 40;

// Цвета Discord
#define CLR_BACKGROUND RGB(32, 34, 37)
#define CLR_CARD RGB(49, 51, 56)
#define CLR_TEXT RGB(220, 221, 222)
#define CLR_TEXT_MUTED RGB(148, 155, 164)
#define CLR_INPUT_BG RGB(56, 58, 64)
#define CLR_BUTTON RGB(88, 101, 242)

LRESULT CALLBACK SettingsWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE: {
            // Создаём шрифт
            g_hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

            // Заголовок "Мой аккаунт"
            HWND hHeader = CreateWindowExW(0, L"Static", L"Мой аккаунт",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                PADDING, PADDING, 300, LABEL_HEIGHT,
                hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            SendMessageW(hHeader, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            // Подзаголовок
            HWND hSubheader = CreateWindowExW(0, L"Static", L"Измените своё отображаемое имя",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                PADDING, PADDING + LABEL_HEIGHT + 4, 400, LABEL_HEIGHT,
                hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            
            HFONT hSmallFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                                           DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
            SendMessageW(hSubheader, WM_SETFONT, (WPARAM)hSmallFont, TRUE);
            // Сохраняем шрифт, чтобы удалить позже
            SetWindowLongPtrW(hSubheader, GWLP_USERDATA, (LONG_PTR)hSmallFont);

            // Метка "Отображаемое имя"
            HWND hLabel = CreateWindowExW(0, L"Static", L"Отображаемое имя",
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                PADDING, PADDING + LABEL_HEIGHT * 2 + 30, 200, LABEL_HEIGHT,
                hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
            SendMessageW(hLabel, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            // Поле ввода
            g_hDisplayNameEdit = CreateWindowExW(
                WS_EX_CLIENTEDGE, L"Edit", Utf8ToWide(g_uiState.userDisplayName).c_str(),
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                PADDING, PADDING + LABEL_HEIGHT * 2 + 30 + LABEL_HEIGHT + 4,
                SETTINGS_WIDTH - PADDING * 2, EDIT_HEIGHT,
                hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL
            );
            SendMessageW(g_hDisplayNameEdit, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            // Кнопка "Сохранить"
            HWND hSaveBtn = CreateWindowExW(
                0, L"Button", L"Сохранить изменения",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                SETTINGS_WIDTH - PADDING - 180, 
                SETTINGS_HEIGHT - PADDING - BUTTON_HEIGHT,
                180, BUTTON_HEIGHT,
                hwnd, (HMENU)1001, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL
            );
            SendMessageW(hSaveBtn, WM_SETFONT, (WPARAM)g_hFont, TRUE);

            return 0;
        }

    case WM_COMMAND: {
        if (LOWORD(wParam) == 1001 && HIWORD(wParam) == BN_CLICKED) {
            wchar_t buffer[256] = {0};
            GetWindowTextW(g_hDisplayNameEdit, buffer, _countof(buffer));
            std::string newName = WideToUtf8(buffer);

            if (!newName.empty()) {
                g_uiState.userDisplayName = newName;
                SendDisplayNameChange(newName); // ← ОТПРАВКА НА СЕРВЕР
            }

            DestroyWindow(hwnd);
        }
        break;
    }

    case WM_CLOSE: {
        DestroyWindow(hwnd);
        return 0;
    }

        case WM_CTLCOLORSTATIC: {
            HDC hdc = (HDC)wParam;
            SetBkColor(hdc, CLR_BACKGROUND);
            // Определяем, какой это статик: заголовок или подзаголовок
            HWND hwndCtrl = (HWND)lParam;
            // Для подзаголовка используем muted цвет
            SetTextColor(hdc, CLR_TEXT_MUTED);
            static HBRUSH hBgBrush = CreateSolidBrush(CLR_BACKGROUND);
            return (INT_PTR)hBgBrush;
        }

        case WM_CTLCOLOREDIT: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, CLR_TEXT);
            SetBkColor(hdc, CLR_INPUT_BG);
            static HBRUSH hBgBrush = CreateSolidBrush(CLR_INPUT_BG);
            return (INT_PTR)hBgBrush;
        }

        case WM_CTLCOLORBTN: {
            HDC hdc = (HDC)wParam;
            SetTextColor(hdc, RGB(255, 255, 255));
            SetBkColor(hdc, CLR_BUTTON);
            static HBRUSH hBgBrush = CreateSolidBrush(CLR_BUTTON);
            return (INT_PTR)hBgBrush;
        }

        case WM_DESTROY: {
            g_hSettingsWnd = NULL;
            // Удаляем шрифты
            if (g_hFont) {
                DeleteObject(g_hFont);
                g_hFont = NULL;
            }
            // Шрифт подзаголовка сохранён в GWLP_USERDATA
            HWND hSubheader = FindWindowExW(hwnd, NULL, L"Static", L"Измените своё отображаемое имя");
            if (hSubheader) {
                HFONT hSmallFont = (HFONT)GetWindowLongPtrW(hSubheader, GWLP_USERDATA);
                if (hSmallFont) DeleteObject(hSmallFont);
            }
            break;
        }

        default:
            return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void OpenSettingsDialog(HWND parent) {
    // Регистрируем класс окна (один раз)
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

    // Создаём окно
    g_hSettingsWnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
        L"AegisSettingsClass",
        L"Настройки — AEGIS",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT,
        SETTINGS_WIDTH, SETTINGS_HEIGHT,
        parent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );

    if (g_hSettingsWnd) {
        // Центрируем относительно родителя
        RECT rcParent, rcDlg;
        GetWindowRect(parent, &rcParent);
        GetWindowRect(g_hSettingsWnd, &rcDlg);
        int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(g_hSettingsWnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);

    }
}
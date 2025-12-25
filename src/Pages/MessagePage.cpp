#include "MessagePage.h"
#include "UIState.h"
#include "Utils/Styles.h"

void DrawMessagePage(HDC hdc, int x, int y, int w, int h) {
    RECT bg = { x, y, x + w, y + h };
    FillRect(hdc, &bg, CreateSolidBrush(RGB(54, 57, 63)));

    HFONT font = CreateAppFont(18, FW_BOLD);
    RECT title = { x + 24, y + 24, x + w, y + 64 };

    DrawTextA(
        hdc,
        g_uiState.activeChatUser.c_str(),
        -1,
        &title,
        DT_LEFT | DT_VCENTER
    );

    DeleteObject(font);
}

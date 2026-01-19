#include "SidebarProfile.h"
#include "Utils/UIState.h"
#include "Utils/Utils.h"
#include <windows.h>
#include <gdiplus.h>
#include <cmath>
#include <filesystem>
#include <string>

extern std::string userAvatar;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace Gdiplus;
namespace fs = std::filesystem;

static Gdiplus::Image* g_pCachedAvatar = nullptr;
static std::string g_lastAvatarPath = "";

#define SIDEBAR_PROFILE_SETTINGS 998

extern int g_hoverIndex;

void DrawSidebarProfile(Graphics& g, int x, int windowHeight, int totalWidth, const std::string& fallbackName) {
    REAL rectX = (REAL)x;
    REAL rectY = (REAL)(windowHeight - PROFILE_PANEL_HEIGHT);
    REAL rectW = (REAL)totalWidth;
    REAL rectH = (REAL)PROFILE_PANEL_HEIGHT;

    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);

    SolidBrush bgBrush(Color(255, 35, 36, 40));
    g.FillRectangle(&bgBrush, rectX, rectY, rectW, rectH);

    float avatarSize = 32.0f;
    float margin = (rectH - avatarSize) / 2.0f;
    RectF avatarRect(rectX + 12.0f, rectY + margin, avatarSize, avatarSize);

    if (userAvatar != g_lastAvatarPath) {
        if (g_pCachedAvatar) {
            delete g_pCachedAvatar;
            g_pCachedAvatar = nullptr;
        }
        g_lastAvatarPath = userAvatar;

        if (!userAvatar.empty() && userAvatar != "[^.^]" && fs::exists(userAvatar)) {
            std::wstring wPath = Utf8ToWide(userAvatar);
            g_pCachedAvatar = Gdiplus::Image::FromFile(wPath.c_str());
        
            if (g_pCachedAvatar && g_pCachedAvatar->GetLastStatus() != Gdiplus::Ok) {
                delete g_pCachedAvatar;
                g_pCachedAvatar = nullptr;
            }
        }
    }

    if (g_pCachedAvatar) {
        GraphicsPath path;
        path.AddEllipse(avatarRect);
        
        GraphicsState state = g.Save();
        g.SetClip(&path);
        g.DrawImage(g_pCachedAvatar, avatarRect);
        g.Restore(state);
    } else {
        SolidBrush avatarBrush(Color(255, 88, 101, 242));
        g.FillEllipse(&avatarBrush, avatarRect);
        
        std::string name = g_uiState.userDisplayName.empty() ? fallbackName : g_uiState.userDisplayName;
        if (!name.empty()) {
            std::wstring firstLetter = Utf8ToWide(name.substr(0, 1));
            FontFamily ff(L"Segoe UI");
            Font letterFont(&ff, 14, FontStyleBold, UnitPixel);
            SolidBrush whiteBrush(Color(255, 255, 255));
            
            StringFormat sf;
            sf.SetAlignment(StringAlignmentCenter);
            sf.SetLineAlignment(StringAlignmentCenter);
            g.DrawString(firstLetter.c_str(), -1, &letterFont, avatarRect, &sf, &whiteBrush);
        }
    }
    std::string nameToDraw = g_uiState.userDisplayName.empty() ? fallbackName : g_uiState.userDisplayName;
    std::wstring wname = Utf8ToWide(nameToDraw);

    FontFamily fontFamily(L"Segoe UI");
    Font nameFont(&fontFamily, 13, FontStyleBold, UnitPixel);
    Font statusFont(&fontFamily, 11, FontStyleRegular, UnitPixel);

    SolidBrush whiteBrush(Color(255, 255, 255));
    SolidBrush grayBrush(Color(255, 185, 187, 190));

    REAL textX = rectX + 12.0f + avatarSize + 10.0f;
    g.DrawString(wname.c_str(), -1, &nameFont, PointF(textX, rectY + (rectH / 2.0f) - 16.0f), &whiteBrush);
    g.DrawString(L"В сети", -1, &statusFont, PointF(textX, rectY + (rectH / 2.0f) + 2.0f), &grayBrush);

    const REAL ICON_SIZE = 24.0f;
    REAL iconRight = rectX + rectW - 12.0f - ICON_SIZE;
    REAL iconTop = rectY + (rectH - ICON_SIZE) / 2.0f;

    bool isHover = (g_hoverIndex == SIDEBAR_PROFILE_SETTINGS);
    
    if (isHover) {
        SolidBrush iconBgBrush(Color(255, 78, 80, 88));
        g.FillEllipse(&iconBgBrush, iconRight, iconTop, ICON_SIZE, ICON_SIZE);
    }

    Pen gearPen(Color(255, 255, 255), 1.8f);
    gearPen.SetLineJoin(LineJoinRound);

    REAL cx = iconRight + ICON_SIZE / 2.0f;
    REAL cy = iconTop + ICON_SIZE / 2.0f;
    REAL rInner = 5.0f;
    REAL rOuter = 9.5f;

    g.DrawEllipse(&gearPen, cx - rOuter, cy - rOuter, rOuter * 2, rOuter * 2);

    for (int i = 0; i < 8; ++i) {
        REAL angle = (REAL)(i * M_PI / 4.0);
        REAL x1 = cx + rInner * cos(angle);
        REAL y1 = cy + rInner * sin(angle);
        REAL x2 = cx + rOuter * cos(angle);
        REAL y2 = cy + rOuter * sin(angle);
        g.DrawLine(&gearPen, x1, y1, x2, y2);
    }
    g.DrawEllipse(&gearPen, cx - rInner, cy - rInner, rInner * 2, rInner * 2);
}

bool IsClickOnProfile(int x, int y, int sidebarX, int windowHeight, int width) {
    int profileY = windowHeight - PROFILE_PANEL_HEIGHT;
    return (x >= sidebarX && x <= sidebarX + width && y >= profileY && y <= windowHeight);
}

bool IsClickOnSettingsIcon(int x, int y, int sidebarX, int windowHeight, int totalWidth) {
    const int ICON_SIZE = 24;
    int profileY = windowHeight - PROFILE_PANEL_HEIGHT;
    int iconRight = sidebarX + totalWidth - 12 - ICON_SIZE;
    int iconTop = profileY + (PROFILE_PANEL_HEIGHT - ICON_SIZE) / 2;

    return (x >= iconRight && x <= iconRight + ICON_SIZE &&
            y >= iconTop && y <= iconTop + ICON_SIZE);
}
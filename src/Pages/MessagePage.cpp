#include "MessagePage.h"
#include "Utils/Styles.h"
#include "Utils/Utils.h"
#include <string>
#include <gdiplus.h>

using namespace Gdiplus;

// Глобальная переменная прокрутки
extern int scrollPos; 

#define COLOR_BG            RGB(49, 51, 56)
#define COLOR_TEXT_MAIN     RGB(219, 222, 225)
#define COLOR_TEXT_MUTED    RGB(148, 155, 164)
#define COLOR_USERNAME      RGB(255, 255, 255)
#define COLOR_AVATAR_BG     RGB(88, 101, 242)


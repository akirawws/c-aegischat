#include "DeveloperBlog.h"
#include <gdiplus.h>
#include <vector>
#include <string>

using namespace Gdiplus;

// Глобальная переменная, которую мы читаем в MainPage для ограничения прокрутки
int g_totalBlogHeight = 0;

struct BlogPatch {
    std::wstring version;
    std::wstring date;
    std::wstring title;
    std::wstring description;
    Color themeColor;
};

void DrawDeveloperBlogPage(HDC memDC, RECT rect, int startX, int scrollOffset) {
    Graphics g(memDC);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    // 1. Рисуем фон (на всю высоту окна)
    SolidBrush bgBrush(Color(255, 49, 51, 56)); 
    g.FillRectangle(&bgBrush, startX, 0, rect.right - startX, rect.bottom);

    // 2. ВАЖНО: Устанавливаем область отсечения (Clipping)
    // Это гарантирует, что текст не будет рисоваться поверх верхней или нижней границы окна при прокрутке
    Rect clipRect(startX, 0, rect.right - startX, rect.bottom);
    g.SetClip(clipRect);

    // Подготовка шрифтов
    FontFamily fontFamily(L"Segoe UI");
    Font titleFont(&fontFamily, 28, FontStyleBold, UnitPixel);
    Font versionFont(&fontFamily, 12, FontStyleBold, UnitPixel);
    Font cardTitleFont(&fontFamily, 18, FontStyleBold, UnitPixel);
    Font dateFont(&fontFamily, 13, FontStyleRegular, UnitPixel);
    Font descFont(&fontFamily, 15, FontStyleRegular, UnitPixel);

    // Кисти
    SolidBrush whiteBrush(Color(255, 255, 255));
    SolidBrush grayBrush(Color(255, 181, 186, 193));
    SolidBrush cardBgBrush(Color(255, 43, 45, 49));

    // Данные постов
    std::vector<BlogPatch> patches = {
        { L"v1.7.0", L"19 ЯНВАРЯ 2026", L"Блог разработчика", L"Реализована страница Developer Blog, где описываются изменения на протяжении всей разработки.", Color(255, 180, 70, 80) },
        { L"v1.6.1", L"18 ЯНВАРЯ 2026", L"Сервер", L"Исправлена утечка GDI, улучшена стабильность туннеля на серверной части, исправлена ошибка при переключении страниц на главном сайдбаре.", Color(255, 180, 70, 80) },
        { L"v1.6.0", L"15 ЯНВАРЯ 2026", L"Страница входа", L"Улучшена страница аутентификации, на клиентской части добавлено создание LastSessionToken.", Color(255, 180, 70, 80) },
        { L"v1.5.4", L"14 ЯНВАРЯ 2026", L"Исправление ошибок", L"Мелкие фиксы для удобства пользователей, кеширование текста, автоскролл при загрузке, подогнали размеры элементов.", Color(255, 180, 70, 80) },
        { L"v1.5.3", L"13 ЯНВАРЯ 2026", L"Улучшение профиля", L"Подключена БД к профилям, добавлена возможность менять ник, био, аватар.", Color(255, 180, 70, 80) },
        { L"v1.5.2", L"1 ЯНВАРЯ 2026", L"Профиль", L"Начата работа над профилями пользователей.", Color(255, 180, 70, 80) },
        { L"v1.5.1", L"31 ДЕКАБРЯ 2025", L"Исправление ошибок", L"Исправлены ошибки, связанные с сохранением сообщений в группах.", Color(255, 180, 70, 80) },
        { L"v1.5.0", L"27 ДЕКАБРЯ 2025", L"Группы", L"Добавлена возможность создавать группы с друзьями до 10 человек, подключены новые таблицы БД.", Color(255, 180, 70, 80) },
        { L"v1.4.0", L"26 ДЕКАБРЯ 2025", L"Личные чаты", L"Добавлена полноценная версия ЛС, улучшен визуал, исправлено множество ошибок.", Color(255, 180, 70, 80) },
        { L"v1.3.0", L"25 ДЕКАБРЯ 2025", L"База данных", L"Подключена PostgreSQL, соленое хеширование паролей, начата разработка ЛС.", Color(255, 180, 70, 80) },
        { L"v1.2.0", L"25 ДЕКАБРЯ 2025", L"Интерфейс", L"Добавлен интерфейс для клиентской части, простые сообщения.", Color(255, 180, 70, 80) },
        { L"v1.1.0", L"24 ДЕКАБРЯ 2025", L"Инициализация", L"Создана первая логика, рабочий сервер и клиентская часть через командную строку.", Color(255, 180, 70, 80) }
    };

    // Настройки отступов
    int startY = 30;    // Начальный отступ сверху
    int margin = 30;    // Отступ слева/справа
    int cardHeight = 130;
    int gap = 20;       // Расстояние между карточками
    int cardWidth = (rect.right - startX) - (margin * 2);

    // Рассчитываем текущую позицию Y с учетом скролла
    // scrollOffset мы вычитаем, чтобы контент "ехал вверх"
    int currentY = startY - scrollOffset;

    // --- Рисуем заголовок страницы ---
    // Проверяем видимость (простая оптимизация)
    if (currentY + 50 > 0 && currentY < rect.bottom) {
        g.DrawString(L"Developer Blog", -1, &titleFont, PointF((REAL)startX + margin, (REAL)currentY), &whiteBrush);
    }
    
    // Сдвигаем Y для первого поста
    currentY += 60; 
    
    // Сохраняем "виртуальную" высоту для заголовка (без скролла)
    int virtualY = startY + 60; 

    // --- Цикл по патчам ---
    for (const auto& patch : patches) {
        
        // Оптимизация: Рисуем, только если карточка попадает в экран
        if (currentY + cardHeight > 0 && currentY < rect.bottom) {
            
            // 1. Фон карточки
            RectF cardRect((REAL)startX + margin, (REAL)currentY, (REAL)cardWidth, (REAL)cardHeight);
            g.FillRectangle(&cardBgBrush, cardRect);

            // 2. Акцентная полоска (цвет версии)
            SolidBrush accentBrush(patch.themeColor);
            g.FillRectangle(&accentBrush, (REAL)startX + margin, (REAL)currentY, 5.0f, cardRect.Height);

            // Координаты текста внутри карточки
            float textLeft = (REAL)startX + margin + 20;
            float row1_Y = (REAL)currentY + 15;
            float row2_Y = (REAL)currentY + 40;
            float row3_Y = (REAL)currentY + 75;

            // 3. Версия и Дата
            g.DrawString(patch.version.c_str(), -1, &versionFont, PointF(textLeft, row1_Y), &accentBrush);
            g.DrawString(patch.date.c_str(), -1, &dateFont, PointF(textLeft + 60, row1_Y - 1), &grayBrush);

            // 4. Заголовок
            g.DrawString(patch.title.c_str(), -1, &cardTitleFont, PointF(textLeft, row2_Y), &whiteBrush);

            // 5. Описание (с ограничением области вывода)
            RectF descBox(textLeft, row3_Y, (REAL)cardWidth - 40, 50.0f);
            g.DrawString(patch.description.c_str(), -1, &descFont, descBox, NULL, &grayBrush);
        }

        // Сдвигаем позицию для следующего элемента
        currentY += (cardHeight + gap);
        
        // Считаем полную высоту контента (как будто скролла нет)
        virtualY += (cardHeight + gap);
    }

    // Сохраняем общую высоту в глобальную переменную (для расчета максимума скролла в WndProc)
    g_totalBlogHeight = virtualY;
}
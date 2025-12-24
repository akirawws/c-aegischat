@echo off
chcp 65001 >nul

:: УКАЖИ ЗДЕСЬ ПУТЬ К НОВОМУ КОМПИЛЯТОРУ
set GCC_PATH=C:\mingw64\bin
set PG_PATH=C:\Program Files\PostgreSQL\18

:: Добавляем новый GCC в начало поиска, чтобы TDM не мешал
set PATH=%GCC_PATH%;%PATH%

echo [STEP 1] Очистка старых файлов...
del /f /q *.o server.exe *.dll

echo [STEP 2] Компиляция (WinLibs 64-bit)...
g++ -c Database.cpp -o Database.o -I"%PG_PATH%\include"
g++ -c server.cpp -o server.o -I"%PG_PATH%\include"

echo [STEP 3] Линковка...
:: Убираем проблемный флаг --large-address-aware, здесь он не нужен
g++ -o server.exe server.o Database.o "%PG_PATH%\lib\libpq.lib" ^
    -static-libgcc -static-libstdc++ ^
    -lws2_32 -lsecur32 -lshell32 -ladvapi32 -lcrypt32

if %ERRORLEVEL% EQU 0 (
    echo [STEP 4] Копирование библиотек...
    copy "%PG_PATH%\bin\libpq.dll" . /Y >nul
    copy "%PG_PATH%\bin\libcrypto-3-x64.dll" . /Y >nul
    copy "%PG_PATH%\bin\libssl-3-x64.dll" . /Y >nul
    copy "%PG_PATH%\bin\libintl-9.dll" . /Y >nul
    copy "%PG_PATH%\bin\libiconv-2.dll" . /Y >nul
    
    echo --------------------------------------------------
    echo [ПРОВЕРКА] Если сейчас покажет False - МЫ ПОБЕДИЛИ!
    powershell -NoProfile -Command "cat server.exe -Raw | Select-String -CaseSensitive 'PE..L' -Quiet"
    echo --------------------------------------------------
    echo Запускай server.exe!
) else (
    echo [!] Ошибка сборки. Посмотри текст выше.
)
pause
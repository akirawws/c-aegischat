@echo off
chcp 65001 >nul

:: Убедись, что путь к НОВОМУ GCC верный
set GCC_PATH=C:\TDM-GCC-64\bin
set PG_PATH=C:\Program Files\PostgreSQL\18

set PATH=%GCC_PATH%;%PATH%

echo [1/3] Очистка...
del /f /q *.o server.exe *.dll

echo [2/3] Компиляция...
g++ -m64 -c server.cpp -I"%PG_PATH%\include"
g++ -m64 -c Database.cpp -I"%PG_PATH%\include"

echo [3/3] Линковка (Динамическая через DLL)...
:: Мы убираем -L и -lpq, и вместо этого передаем путь к самой DLL как объектный файл
g++ -m64 server.o Database.o -o server.exe ^
    "%PG_PATH%\bin\libpq.dll" ^
    -lws2_32 -lsecur32 -ladvapi32 -lshell32 -luser32 -lgdi32
    
if %ERRORLEVEL% EQU 0 (
    echo [OK] Собрано! Копируем DLL...
    copy "%PG_PATH%\bin\libpq.dll" . /Y >nul
    copy "%PG_PATH%\bin\libcrypto-3-x64.dll" . /Y >nul
    copy "%PG_PATH%\bin\libssl-3-x64.dll" . /Y >nul
    copy "%PG_PATH%\bin\libintl-9.dll" . /Y >nul
    copy "%PG_PATH%\bin\libiconv-2.dll" . /Y >nul
    
    echo ---------------------------------------
    echo ВСЁ ГОТОВО!
    echo Теперь точно запускай: server.exe
    echo ---------------------------------------
) else (
    echo [!] Ошибка линковки всё еще есть.
)
pause
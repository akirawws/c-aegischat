@echo off
echo Building AEGIS Client...

:: Укажи правильный путь к установленной Postgres
set PG_PATH=C:\Program Files\PostgreSQL\18

g++ -o Aegis.exe ^
    client_gui.cpp ^
    Utils/Styles.cpp ^
    Utils/Utils.cpp ^
    Utils/Network.cpp ^
    Utils/Keyboard.cpp ^
    Utils/HashPassword.cpp ^
    Components/MessageInput.cpp ^
    Components/Sidebar.cpp ^
    Components/MessageList.cpp ^
    Pages/AuthPage.cpp ^
    Pages/MainPage.cpp ^
    Pages/FriendsPage.cpp ^
    -I"%PG_PATH%\include" ^
    -I. ^
    "%PG_PATH%\bin\libpq.dll" ^
    -lws2_32 -lgdi32 -lcomctl32 -lmsimg32 -lbcrypt -mwindows -std=c++17

if %ERRORLEVEL% == 0 (
    echo Build successful!
    copy "%PG_PATH%\bin\libpq.dll" . /Y >nul
) else (
    echo Build failed!
    pause
)
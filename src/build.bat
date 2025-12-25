@echo off
echo Building AEGIS Client...

set PG_PATH=C:\Program Files\PostgreSQL\18

g++ -o Aegis.exe ^
    client_gui.cpp ^
    Utils/Styles.cpp ^
    Utils/Utils.cpp ^
    Utils/Network.cpp ^
    Utils/Keyboard.cpp ^
    Utils/HashPassword.cpp ^
    Utils/FriendsUtils.cpp ^
    Utils/UIState.cpp ^
    Components/MessageInput.cpp ^
    Components/Sidebar.cpp ^
    Components/MessageList.cpp ^
    Components/SidebarFriends.cpp ^
    Pages/AuthPage.cpp ^
    Pages/MainPage.cpp ^
    Pages/FriendsPage.cpp ^
    Pages/MessagePage.cpp ^
    -I"%PG_PATH%\include" ^
    -I. ^
    -IUtils ^
    -IPages ^
    -IComponents ^
    -lws2_32 -lgdi32 -lcomctl32 -lmsimg32 -lbcrypt ^
    -mwindows -std=c++17 ^
    -static -static-libgcc -static-libstdc++

if %ERRORLEVEL% == 0 (
    echo Build successful!
    echo Running Aegis...
    start Aegis.exe
) else (
    echo Build failed!
    pause
)
@echo off
echo Building AEGIS Client...

g++ -o Aegis.exe ^
    client_gui.cpp ^
    Utils/Styles.cpp ^
    Utils/Utils.cpp ^
    Utils/Network.cpp ^
    Utils/Keyboard.cpp ^
    Components/MessageInput.cpp ^
    Components/Sidebar.cpp ^
    Components/MessageList.cpp ^
    Pages/ConnectPage.cpp ^
    Pages/MainPage.cpp ^
    Pages/FriendsPage.cpp ^
    -I"C:/Program Files/PostgreSQL/16/include" ^
    -L"C:/Program Files/PostgreSQL/16/lib" ^
    -I. ^
    -lws2_32 -lgdi32 -lcomctl32 -lmsimg32 -mwindows -std=c++17

if %ERRORLEVEL% == 0 (
    echo Build successful! Output: Aegis.exe
) else (
    echo Build failed!
    pause
)
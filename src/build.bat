@echo off
setlocal enabledelayedexpansion
title Building AEGIS Client...


set PG_PATH=C:\Program Files\PostgreSQL\18
set OUTPUT=Aegis.exe
set FLAGS=-std=c++17 -D_WIN32_WINNT=0x0601 -I"%PG_PATH%\include" -I. -IUtils -IPages -IComponents
set LIBS=-lws2_32 -lgdiplus -lgdi32 -lcomctl32 -lmsimg32 -lbcrypt -lole32 -luuid -mwindows -static -static-libgcc -static-libstdc++

echo ======================================================
echo           BUILDING AEGIS CLIENT PROJECT
echo ======================================================
echo.

set SOURCES=client_gui.cpp Utils/Styles.cpp Utils/Utils.cpp Utils/Network.cpp Utils/Keyboard.cpp Utils/HashPassword.cpp Utils/FriendsUtils.cpp Utils/UIState.cpp Components/MessageInput.cpp Components/Sidebar.cpp Components/MessageList.cpp Components/SidebarFriends.cpp Components/SidebarProfile.cpp Pages/AuthPage.cpp Pages/MainPage.cpp Pages/FriendsPage.cpp Pages/MessagePage.cpp

set OBJECTS=
echo [1/2] Compiling source files...
echo ------------------------------------------------------


for %%F in (%SOURCES%) do (
    echo [COMPILE] %%F...
    g++ -c %%F -o "%%~nF.o" %FLAGS%
    if !ERRORLEVEL! neq 0 (
        echo.
        echo [!] Error during compilation of %%F
        pause
        exit /b !ERRORLEVEL!
    )
    set OBJECTS=!OBJECTS! "%%~nF.o"
)

echo.
echo [2/2] Linking objects into executable...
echo ------------------------------------------------------
echo [LINK] Creating %OUTPUT%...

g++ -o %OUTPUT% %OBJECTS% -L"%PG_PATH%\lib" %LIBS%

if %ERRORLEVEL% == 0 (
    echo.
    echo ======================================================
    echo [SUCCESS] Build completed: %OUTPUT%
    echo ======================================================
    
    del *.o
    
    echo Running Aegis...
    start %OUTPUT%
) else (
    echo.
    echo ======================================================
    echo [FAILED] Build failed! Check errors above.
    echo ======================================================
    pause
)
@echo off
chcp 65001 >nul
title SSH Tunnel Reconnector

:loop
cls
echo [%date% %time%] Остановка предыдущих сессий SSH...
taskkill /f /im ssh.exe >nul 2>&1

echo [%date% %time%] Запуск SSH туннеля...
start "SSH_PROCESS" /b ssh -p 223 -o "ServerAliveInterval 30" -o "ServerAliveCountMax 3" AOCT8OvZxzqFm1xFtCvp_vy9gXx7FaMojYzheI6s-mFiXvIC2MQIkQj2CMicdyW1kowKpm_hwA7bHveUqznPpQ@xisyrurdm.localto.net -R 6162:127.0.0.1:5555

echo [%date% %time%] Туннель запущен. Ожидание 25 минут перед перезапуском...
timeout /t 1500 /nobreak

echo [%date% %time%] Время вышло. Идем на новый круг...
goto loop
@echo off
:: Очищаем PATH для текущего окна, чтобы сервер видел ТОЛЬКО свои библиотеки
set PATH=%CD%;C:\Windows\system32;C:\Windows;C:\Windows\System32\Wbem
echo [RUN] Запуск сервера в изолированном окружении...
server.exe
pause
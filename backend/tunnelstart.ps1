chcp 65001 | Out-Null

$sshCommand = @"
ssh -p 223 -o "ServerAliveInterval 30" -o "ServerAliveCountMax 3" AOCT8OvZxzqFm1xFtCvp_vy9gXx7FaMojYzheI6s-mFiXvIC2MQIkQj2CMicdyW1kowKpm_hwA7bHveUqznPpQ@xisyrurdm.localto.net -R 6162:127.0.0.1:5555
"@

while ($true) {

    Write-Host "Остановка предыдущего SSH (если есть)..."
    Get-Process ssh -ErrorAction SilentlyContinue | Stop-Process -Force

    Start-Sleep -Seconds 2

    Write-Host "Запуск SSH..."
    Invoke-Expression $sshCommand

    Write-Host "SSH завершился. Ожидание 25 минут..."
    Start-Sleep -Seconds (25 * 60)
}

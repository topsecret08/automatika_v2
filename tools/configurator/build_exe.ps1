# Сборка Automatika.exe. Запускать из корня репозитория:
#   powershell -ExecutionPolicy Bypass -File tools\configurator\build_exe.ps1
$ErrorActionPreference = "Stop"
python -m pip install -r tools/configurator/requirements.txt pyinstaller
python -m PyInstaller --noconfirm --onefile --windowed --name Automatika `
    --paths . tools/automatika_app.py
Write-Host "Готово: dist\Automatika.exe"

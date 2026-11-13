# Сборка Automatika V0.5 alfa в PowerShell.
#
#   .\tools\bootstrap.ps1 build   # прошивка
#   .\tools\bootstrap.ps1 test    # тестовый проект
#   .\tools\bootstrap.ps1 all     # прошивка и тесты
#   .\tools\bootstrap.ps1 clean   # очистка
#
# Перед запуском активируйте ESP-IDF 5.3.2:
#   C:\Espressif\frameworks\esp-idf-v5.3.2\export.ps1
param([ValidateSet('all', 'build', 'test', 'clean')][string]$Action = 'all')

$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

function Invoke-Idf {
    param([Parameter(ValueFromRemainingArguments = $true)][string[]]$IdfArgs)
    & idf.py @IdfArgs
    if ($LASTEXITCODE -ne 0) { throw "idf.py $($IdfArgs -join ' ') завершился с кодом $LASTEXITCODE" }
}

if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    throw 'idf.py не найден. Активируйте ESP-IDF: C:\Espressif\frameworks\esp-idf-v5.3.2\export.ps1'
}

switch ($Action) {
    'build' {
        if (-not (Test-Path 'sdkconfig')) { Invoke-Idf set-target esp32s3 }
        Invoke-Idf build
        Invoke-Idf size
    }
    'test' {
        if (-not (Test-Path 'test/sdkconfig')) { Invoke-Idf -C test set-target esp32s3 }
        Invoke-Idf -C test build
    }
    'all' {
        if (-not (Test-Path 'sdkconfig')) { Invoke-Idf set-target esp32s3 }
        Invoke-Idf build
        Invoke-Idf size
        if (-not (Test-Path 'test/sdkconfig')) { Invoke-Idf -C test set-target esp32s3 }
        Invoke-Idf -C test build
    }
    'clean' {
        Remove-Item -Recurse -Force build, test/build, sdkconfig, sdkconfig.old,
            test/sdkconfig, test/sdkconfig.old, managed_components,
            dependencies.lock -ErrorAction SilentlyContinue
    }
}

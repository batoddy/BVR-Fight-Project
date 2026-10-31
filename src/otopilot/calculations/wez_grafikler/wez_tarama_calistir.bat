@echo off
chcp 65001 > nul
rem WEZ ve R_cold taramasi. Cift tikla calistir.
rem 1) Kisa kontrol (bu pencerede), 2) dort tarama ayri pencerelerde ayni anda.
rem Sonuclar bu klasordeki CSV dosyalarina yazilir; yarida kalirsa tekrar calistir, kaldigi yerden devam eder.

cd /d "%~dp0..\..\..\.."
set PY=.venv\Scripts\python.exe
set BETIK=src\otopilot\calculations\wez_grafikler\wez_tarama.py

if not exist "%PY%" (
    echo .venv bulunamadi: %CD%\%PY%
    pause
    exit /b 1
)

echo === 1/2 Kontrol: tek yonluluk ===
"%PY%" -X utf8 "%BETIK%" --kontrol
if errorlevel 1 (
    echo.
    echo Kontrolde hata cikti. Bu pencerenin ciktisini Claude'a gonder.
    pause
    exit /b 1
)

echo.
echo === 2/2 Taramalar ayri pencerelerde basliyor ===
start "WEZ hot"       cmd /k ""%PY%" -X utf8 "%BETIK%" --aspect hot"
start "WEZ beam"      cmd /k ""%PY%" -X utf8 "%BETIK%" --aspect beam"
start "WEZ cold"      cmd /k ""%PY%" -X utf8 "%BETIK%" --aspect cold"
start "WEZ atan hizi" cmd /k ""%PY%" -X utf8 "%BETIK%" --grup atan_hizi"

echo Dort pencere acildi. Hepsi bitince Claude'a "bitti" de.
pause

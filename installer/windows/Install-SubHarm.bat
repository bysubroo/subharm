@echo off
rem SubHarm installer: double-click. Asks for admin once, copies SubHarm.vst3 to the shared VST3 folder.
net session >nul 2>&1
if errorlevel 1 (
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)
set "SRC=%~dp0SubHarm.vst3"
set "DST=%CommonProgramFiles%\VST3"
if not exist "%SRC%" (
  echo Nije nadjen SubHarm.vst3 pored ovog fajla. Prvo raspakuj ceo zip u folder.
  pause
  exit /b 1
)
if not exist "%DST%" mkdir "%DST%"
if exist "%DST%\SubHarm.vst3" rmdir /s /q "%DST%\SubHarm.vst3"
xcopy "%SRC%" "%DST%\SubHarm.vst3\" /e /i /y /q >nul
if errorlevel 1 (
  echo Kopiranje nije uspelo.
  pause
  exit /b 1
)
echo.
echo SubHarm je instaliran u %DST%\SubHarm.vst3
echo Rescan plugina: FL Studio - Options ^> Manage plugins ^> Find more plugins
echo                 Cubase    - Studio ^> VST Plug-in Manager ^> Rescan
echo.
pause

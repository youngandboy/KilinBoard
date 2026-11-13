@echo off
setlocal

rem ---------------------------------------------------------------------------
rem Flash OneNet_Mini_Bare.hex with ST-LINK_CLI.exe (no GUI).
rem
rem Usage:
rem   flash.bat
rem   flash.bat -Full
rem   flash.bat "path\to\firmware.hex"
rem   flash.bat -Full "path\to\firmware.hex"
rem
rem Default hex:
rem   output\gcc\OneNet_Mini_Bare.hex      (ARM GCC build)
rem ---------------------------------------------------------------------------

set "FULL="
set "HEX="
set "CLI=%STLINK_CLI%"

if /I "%~1"=="-Full" (
    set "FULL=1"
    set "HEX=%~2"
) else (
    set "HEX=%~1"
)

if not defined HEX set "HEX=%~dp0output\gcc\OneNet_Mini_Bare.hex"

if not exist "%HEX%" (
    echo ERROR: HEX file not found:
    echo   %HEX%
    echo Build the project first: build.bat
    exit /b 1
)

if not defined CLI if exist "C:\Program Files (x86)\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe" set "CLI=C:\Program Files (x86)\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe"
if not defined CLI if exist "C:\Program Files\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe" set "CLI=C:\Program Files\STMicroelectronics\STM32 ST-LINK Utility\ST-LINK Utility\ST-LINK_CLI.exe"
if not defined CLI for %%I in (ST-LINK_CLI.exe) do if not "%%~$PATH:I"=="" set "CLI=%%~$PATH:I"

if not defined CLI (
    echo ERROR: ST-LINK_CLI.exe not found.
    echo Set STLINK_CLI to the full path of ST-LINK_CLI.exe.
    exit /b 1
)

rem The GUI holds the probe exclusively. Refuse to start a confusing CLI failure.
tasklist /FI "IMAGENAME eq STM32 ST-LINK Utility.exe" 2>nul | find /I "STM32 ST-LINK Utility.exe" >nul
if not errorlevel 1 (
    echo WARNING: STM32 ST-LINK Utility GUI is running.
    echo Close it first, otherwise ST-LINK_CLI reports "ST-LINK Already Used".
    exit /b 1
)

set "ERASE="
if defined FULL set "ERASE=-ME"

set "MODE=SWD"
if defined FULL set "MODE=SWD + full chip erase"

echo CLI : %CLI%
echo HEX : %HEX%
echo Mode: %MODE%
echo.

"%CLI%" -c SWD %ERASE% -P "%HEX%" -V while_programming -Rst -Run -Q -NoPrompt
set "RC=%ERRORLEVEL%"

if "%RC%"=="0" (
    echo.
    echo Flash OK
) else (
    echo.
    echo Flash FAILED, exit code %RC%
)

exit /b %RC%

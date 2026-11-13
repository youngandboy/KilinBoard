@echo off
setlocal

rem ---------------------------------------------------------------------------
rem Build the Keil project from the command line.
rem
rem Important: if ARM ADS 1.2 is installed, its ARMCONF/ARMDLL/ARMHOME/ARMINC/
rem ARMLIB variables make Keil ARMCC5 pick up the old ADS stdarg.h. That causes
rem the confusing error:
rem   usart.c(190): error: #928: incorrect use of va_start
rem Clear them for this process before starting UV4.
rem ---------------------------------------------------------------------------
set "ARMCONF="
set "ARMDLL="
set "ARMHOME="
set "ARMINC="
set "ARMLIB="

set "UV4=%KEIL_UV4%"
if not defined UV4 if exist "K:\Keil_v5\UV4\UV4.exe" set "UV4=K:\Keil_v5\UV4\UV4.exe"
if not defined UV4 if exist "C:\Keil_v5\UV4\UV4.exe" set "UV4=C:\Keil_v5\UV4\UV4.exe"
if not defined UV4 if exist "C:\Keil\UV4\UV4.exe" set "UV4=C:\Keil\UV4\UV4.exe"
if not defined UV4 if exist "%ProgramFiles%\Keil_v5\UV4\UV4.exe" set "UV4=%ProgramFiles%\Keil_v5\UV4\UV4.exe"
if not defined UV4 if exist "%ProgramFiles(x86)%\Keil_v5\UV4\UV4.exe" set "UV4=%ProgramFiles(x86)%\Keil_v5\UV4\UV4.exe"
if not defined UV4 for %%I in (UV4.exe) do if not "%%~$PATH:I"=="" set "UV4=%%~$PATH:I"

if not defined UV4 (
    echo ERROR: UV4.exe not found.
    echo Set KEIL_UV4 to the full path of UV4.exe, or add Keil to PATH.
    exit /b 1
)

set "PROJ=%~dp0OneNet_Mini_Bare.uvprojx"
set "LOG=%~dp0keil_build.log"
set "BUILD_OPT=-b"

if /I "%~1"=="-Rebuild" set "BUILD_OPT=-r"

echo Using UV4: %UV4%
"%UV4%" %BUILD_OPT% "%PROJ%" -j0 -o "%LOG%"
set "RC=%ERRORLEVEL%"

if exist "%LOG%" type "%LOG%"

if "%RC%"=="0" (
    echo.
    echo Keil build OK
    echo   HEX: %~dp0output\OneNet_Mini_Bare.hex
) else (
    echo.
    echo Keil build FAILED, exit code %RC%
)

exit /b %RC%

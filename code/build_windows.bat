@echo off
rem Compila no Windows com MSVC (cl.exe). Nao usa winpthreads, so Win32.
rem Pode rodar de qualquer terminal: se o cl nao estiver no PATH, o
rem script procura o Visual Studio / Build Tools e carrega o ambiente.
rem
rem   build_windows.bat        -> compila
rem   build_windows.bat test   -> compila e roda os testes

setlocal
cd /d "%~dp0"

where cl >nul 2>&1
if not errorlevel 1 goto compilar

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto sem_msvc
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto sem_msvc
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
where cl >nul 2>&1
if errorlevel 1 goto sem_msvc

:compilar
set FLAGS=/nologo /O2 /W4 /D_CRT_SECURE_NO_WARNINGS
set LIB_SRC=platform_thread.c bbuffer_cond.c bbuffer_sem.c

cl %FLAGS% /Febench.exe bench.c %LIB_SRC% || goto erro
cl %FLAGS% /I. /Fetest_platform.exe ..\testes\test_platform.c platform_thread.c || goto erro
cl %FLAGS% /I. /Fetest_bbuffer.exe ..\testes\test_bbuffer.c %LIB_SRC% || goto erro
del /q *.obj 2>nul

if /i "%~1"=="test" (
    .\test_platform.exe || goto erro
    .\test_bbuffer.exe  || goto erro
)

echo.
echo Compilado: bench.exe test_platform.exe test_bbuffer.exe
exit /b 0

:sem_msvc
echo Nao encontrei o MSVC. Instale o "Visual Studio Build Tools" com a
echo carga de trabalho "Desenvolvimento para desktop com C++".
exit /b 1

:erro
echo Falhou.
exit /b 1

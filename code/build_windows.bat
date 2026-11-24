@echo off
rem Compila no Windows com MSVC (cl.exe).
rem Rodar dentro do "x64 Native Tools Command Prompt for VS".
rem Nao usa winpthreads: so a API Win32.
rem
rem   build_windows.bat        -> compila
rem   build_windows.bat test   -> compila e roda os testes

setlocal
cd /d "%~dp0"

set FLAGS=/nologo /O2 /W4 /D_CRT_SECURE_NO_WARNINGS
set LIB_SRC=platform_thread.c bbuffer_cond.c bbuffer_sem.c

cl %FLAGS% /Febench.exe bench.c %LIB_SRC% || goto erro
cl %FLAGS% /I. /Fetest_platform.exe ..\testes\test_platform.c platform_thread.c || goto erro
cl %FLAGS% /I. /Fetest_bbuffer.exe ..\testes\test_bbuffer.c %LIB_SRC% || goto erro
del /q *.obj 2>nul

if "%1"=="test" (
    test_platform.exe || goto erro
    test_bbuffer.exe  || goto erro
)

echo.
echo Compilado: bench.exe test_platform.exe test_bbuffer.exe
exit /b 0

:erro
echo Falhou.
exit /b 1

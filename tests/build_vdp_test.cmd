@echo off
rem Run from an x86 Visual Studio Developer Command Prompt.
rc /nologo /fo "%TEMP%\vdp_test.res" src\Gens\gens.rc
if errorlevel 1 exit /b 1
cl /nologo /TC tests\vdp_window_test.c /Fo"%TEMP%\vdp_window_test.obj" /Fe"%TEMP%\vdp_window_test.exe" /link "%TEMP%\vdp_test.res" user32.lib gdi32.lib comdlg32.lib
if errorlevel 1 exit /b 1
"%TEMP%\vdp_window_test.exe"

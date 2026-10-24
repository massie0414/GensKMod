@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=x86 >nul
rc /nologo /fo "%TEMP%\vdp32x_test.res" src\Gens\gens.rc
if errorlevel 1 exit /b 1
cl /nologo /O2 /MT /TC tests\vdp32x_window_test.c /Fo"%TEMP%\vdp32x_window_test.obj" /Fe"%TEMP%\vdp32x_window_test.exe" /link "%TEMP%\vdp32x_test.res" user32.lib gdi32.lib comdlg32.lib comctl32.lib
if errorlevel 1 exit /b 1
"%TEMP%\vdp32x_window_test.exe"

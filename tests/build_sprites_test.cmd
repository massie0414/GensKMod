@echo off
rem Run from an x86 Visual Studio Developer Command Prompt.
rc /nologo /fo "%TEMP%\sprites_test.res" src\Gens\gens.rc
if errorlevel 1 exit /b 1
cl /nologo /TC tests\sprites_window_test.c /Fo"%TEMP%\sprites_window_test.obj" /Fe"%TEMP%\sprites_window_test.exe" /link "%TEMP%\sprites_test.res" user32.lib gdi32.lib comdlg32.lib comctl32.lib
if errorlevel 1 exit /b 1
"%TEMP%\sprites_window_test.exe"

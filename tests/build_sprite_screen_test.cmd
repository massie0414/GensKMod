@echo off
rem Run from an x86 Visual Studio Developer Command Prompt.
rc /nologo /fo "%TEMP%\sprite_screen.res" src\Gens\gens.rc
if errorlevel 1 exit /b 1
cl /nologo /TC tests\sprite_screen_test.c /Fo"%TEMP%\sprite_screen_test.obj" /Fe"%TEMP%\sprite_screen_test.exe" /link "%TEMP%\sprite_screen.res" user32.lib gdi32.lib
if errorlevel 1 exit /b 1
"%TEMP%\sprite_screen_test.exe"
if errorlevel 1 exit /b 1
cl /nologo /TC tests\plane_explorer_test.c /Fo"%TEMP%\plane_explorer_test.obj" /Fe"%TEMP%\plane_explorer_test.exe" /link "%TEMP%\sprite_screen.res" user32.lib gdi32.lib
if errorlevel 1 exit /b 1
"%TEMP%\plane_explorer_test.exe"
if errorlevel 1 exit /b 1

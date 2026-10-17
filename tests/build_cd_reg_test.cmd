@echo off
rem Run from an x86 Visual Studio Developer Command Prompt after building Release.
cl /nologo /O2 /MT /TC tests\cd_reg_window_test.c /Fo"src\Gens\bin\cd_reg_window_test.obj" /Fe"src\Gens\bin\cd_reg_window_test.exe" /link src\Gens\bin\obj\Release\Gens.res user32.lib comctl32.lib
if errorlevel 1 exit /b 1
src\Gens\bin\cd_reg_window_test.exe

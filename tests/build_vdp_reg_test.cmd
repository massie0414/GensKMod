@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
cl /nologo /O2 /MT /TC tests\vdp_reg_window_test.c /Fo"src\Gens\bin\vdp_reg_window_test.obj" /Fe"src\Gens\bin\vdp_reg_window_test.exe" /link src\Gens\bin\obj\Release\Gens.res user32.lib comctl32.lib
if errorlevel 1 exit /b 1
src\Gens\bin\vdp_reg_window_test.exe
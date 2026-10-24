@call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars32.bat" >nul
@cl /nologo /TC tests\plane_explorer_test.c /Fo"C:\Users\massi\AppData\Local\Packages\sandbox.{d6be3277-46c9-4199-880e-e3a0f94604f4}\AC\Temp\plane_explorer_test.obj" /Fe"C:\Users\massi\AppData\Local\Packages\sandbox.{d6be3277-46c9-4199-880e-e3a0f94604f4}\AC\Temp\plane_explorer_test.exe" /link src\Gens\bin\obj\Release\Gens.res user32.lib gdi32.lib
@if errorlevel 1 exit /b 1
@"C:\Users\massi\AppData\Local\Packages\sandbox.{d6be3277-46c9-4199-880e-e3a0f94604f4}\AC\Temp\plane_explorer_test.exe"

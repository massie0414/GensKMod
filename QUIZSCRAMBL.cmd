@echo off
setlocal
pushd "%~dp0src\Gens\bin\Release"
start "" "%~dp0src\Gens\bin\Release\gens.exe" "%~dp0rom\QUIZSCRAMBL\QUIZSCRAMBL.CUE"
popd

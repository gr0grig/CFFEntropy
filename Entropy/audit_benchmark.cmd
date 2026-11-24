@echo off
setlocal
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
cd /d "%~dp0"
cl /nologo /std:c++17 /O2 /W4 /EHsc /MT audit_benchmark.cpp /Fo:build\audit_benchmark.obj /Fe:build\audit_benchmark.exe /link build\Entropy.res user32.lib comctl32.lib gdi32.lib
if errorlevel 1 exit /b 1
build\audit_benchmark.exe

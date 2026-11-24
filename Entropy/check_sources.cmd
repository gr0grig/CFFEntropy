@echo off
setlocal
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist build\check mkdir build\check
rc /nologo /fo build\check\Entropy.res Entropy.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /W4 /EHsc /MT /c Entropy.cpp /Fo:build\check\Entropy.obj
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /W4 /EHsc /MT tests.cpp /Fo:build\check\tests.obj /Fe:build\check\tests.exe
if errorlevel 1 exit /b 1
build\check\tests.exe
exit /b %errorlevel%

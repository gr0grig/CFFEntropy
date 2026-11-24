@echo off
setlocal
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cd /d "%~dp0"
if not exist build mkdir build
rc /nologo /fo build\Entropy.res Entropy.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /W4 /EHsc /MT /LD Entropy.cpp /Fo:build\Entropy.obj /link /OUT:build\Entropy.dll /IMPLIB:build\Entropy.lib build\Entropy.res user32.lib comctl32.lib /DYNAMICBASE /NXCOMPAT
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /W4 /EHsc /MT tests.cpp /Fo:build\tests.obj /Fe:build\tests.exe
if errorlevel 1 exit /b 1
build\tests.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /W4 /EHsc /MT smoke_test.cpp /Fo:build\smoke_test.obj /Fe:build\smoke_test.exe /link user32.lib comctl32.lib
if errorlevel 1 exit /b 1
build\smoke_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /W4 /EHsc /MT scope_test.cpp /Fo:build\scope_test.obj /Fe:build\scope_test.exe /link build\Entropy.res user32.lib comctl32.lib
if errorlevel 1 exit /b 1
build\scope_test.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /O2 /W4 /EHsc /MT mouse_stress_test.cpp /Fo:build\mouse_stress_test.obj /Fe:build\mouse_stress_test.exe /link build\Entropy.res user32.lib comctl32.lib
if errorlevel 1 exit /b 1
build\mouse_stress_test.exe
exit /b %errorlevel%

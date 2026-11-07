@echo off
rem Builds and runs the schedule unit tests with Visual Studio (x64).
rem Needs the dependency bundle staged by build_scripts\msw\build_xSchedule_x64.cmd and the VS 2026 toolset.
setlocal
cd /d "%~dp0"

rem the "(x86)" in the path would end the for block early, so expand it outside the block
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`call "%VSWHERE%" -version [18.0^,19.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set VSINSTALL=%%i
if "%VSINSTALL%"=="" (
    echo Visual Studio 2026 with C++ tools not found
    exit /b 2
)
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

set WX=..\..\xlights\dependencies-bundle\wxWidgets
if not exist build mkdir build
cl /nologo /std:c++20 /EHsc /MD /O2 /utf-8 /W3 /DNDEBUG /DUNICODE /D_UNICODE /D__MSVC__ /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS ^
   /I%WX%\include /I%WX%\include\msvc ^
   /I..\..\xlights\include /I..\..\xlights\src-core /I..\..\xlights\src-core\utils /I..\..\xlights\src-ui-wx /I..\..\xlights\src-ui-wx\shared\utils /I..\..\xlights\common ^
   /I..\..\dependencies /I..\..\dependencies\spdlog\include ^
   /Fobuild\ /Fe:build\xScheduleTests.exe ^
   ScheduleTests.cpp ..\Schedule.cpp ..\SchedulePreview.cpp ..\ShowBackups.cpp ..\City.cpp ..\Holidays.cpp ^
   /link /LIBPATH:%WX%\lib\vc_x64_lib /SUBSYSTEM:CONSOLE
if errorlevel 1 exit /b 1

build\xScheduleTests.exe

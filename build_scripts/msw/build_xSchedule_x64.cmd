set cwd=%CD%

rem Ask vswhere for MSBuild rather than naming Visual Studio install paths.
rem The projects need the v145 toolset (the dependency bundle is built with it).
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist %VSWHERE% goto NoVsWhere
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\amd64\MSBuild.exe`) do set MSBUILD_DIR=%%~dpi
if not defined MSBUILD_DIR goto NoVsWhere
set PATH=%MSBUILD_DIR%;%PATH%
echo Using MSBuild from %MSBUILD_DIR%
goto Start

:NoVsWhere
echo vswhere found no MSBuild - relying on whatever is already on PATH

:Start

cd ..
cd ..

rem Stage the prebuilt dependency bundle (wxWidgets, FFmpeg, curl, SDL2, ...) pinned by
rem xlights\ci_scripts\deps_version.txt, plus the VC++ redistributable for the installer.
powershell -NoProfile -ExecutionPolicy Bypass -File xlights\ci_scripts\fetch_dependencies.ps1 -Only Dependencies
if %ERRORLEVEL% NEQ 0 goto error
powershell -NoProfile -ExecutionPolicy Bypass -File xlights\ci_scripts\fetch_dependencies.ps1 -Only VCRedist
if %ERRORLEVEL% NEQ 0 goto error

cd xSchedule
msbuild.exe -m:10 xSchedule.sln -p:Configuration="Release" -p:Platform="x64"
if %ERRORLEVEL% NEQ 0 goto error

cd xSMSDaemon
msbuild.exe -m:10 xSMSDaemon.sln -p:Configuration="Release" -p:Platform="x64"
if %ERRORLEVEL% NEQ 0 goto error
cd ..

cd RemoteFalcon
msbuild.exe -m:10 RemoteFalcon.sln -p:Configuration="Release" -p:Platform="x64"
if %ERRORLEVEL% NEQ 0 goto error
cd ..

cd ..

cd build_scripts
cd msw

goto exit

:error

@echo Error compiling xSchedule x64
pause
exit 1

:exit

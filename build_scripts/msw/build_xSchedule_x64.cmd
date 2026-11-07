@echo off
rem Builds xSchedule and its plugins (Release x64). Needs the VS 2026 (v145) toolset, any edition including
rem Build Tools: the xLights dependency bundle it fetches is built with it.

set cwd=%CD%
cd /d "%~dp0..\.."

powershell -NoProfile -ExecutionPolicy Bypass -File xlights\ci_scripts\fetch_dependencies.ps1 -Only Dependencies
if %ERRORLEVEL% NEQ 0 goto error
powershell -NoProfile -ExecutionPolicy Bypass -File xlights\ci_scripts\fetch_dependencies.ps1 -Only VCRedist
if %ERRORLEVEL% NEQ 0 goto error

set MSBUILD=
for /f "usebackq delims=" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -version [18.0^,19.0^) -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\amd64\MSBuild.exe`) do (
    if not defined MSBUILD set "MSBUILD=%%i"
)
if not defined MSBUILD (
    echo Visual Studio 2026 ^(or its Build Tools^) is needed to build against the xLights dependency bundle.
    goto error
)
echo Using %MSBUILD%

"%MSBUILD%" -m xSchedule\xSchedule.sln -p:Configuration="Release" -p:Platform="x64"
if %ERRORLEVEL% NEQ 0 goto error

"%MSBUILD%" -m xSchedule\xSMSDaemon\xSMSDaemon.sln -p:Configuration="Release" -p:Platform="x64"
if %ERRORLEVEL% NEQ 0 goto error

"%MSBUILD%" -m xSchedule\RemoteFalcon\RemoteFalcon.sln -p:Configuration="Release" -p:Platform="x64"
if %ERRORLEVEL% NEQ 0 goto error

cd /d "%cwd%"
goto exit

:error

@echo Error compiling xSchedule x64
cd /d "%cwd%"
pause
exit /b 1

:exit

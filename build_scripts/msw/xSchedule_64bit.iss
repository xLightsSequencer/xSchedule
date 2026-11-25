; -- xSchedule_64bit.iss --
; Installer for xSchedule standalone

#include "xSchedule_common.iss"

[Setup]
ChangesEnvironment=yes
DisableDirPage=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

AppName={#MyTitleName}
AppVersion={#Year}.{#Version}{#Other}
DefaultDirName={commonpf64}\{#MyTitleName}{#Other}
DefaultGroupName={#MyTitleName}{#Other}
SetupIconFile=..\..\xlights\include\xSchedule64.ico
UninstallDisplayIcon={app}\{#MyTitleName}.exe
Compression=lzma2
SolidCompression=yes
OutputDir=output
OutputBaseFilename={#MyTitleName}_{#Year}_{#Version}{#Other}

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "Do you want to create desktop icon?"; Flags: checkablealone

[Files]
; xSchedule
Source: "../../xSchedule/x64/Release/xSchedule.exe"; DestDir: "{app}"
Source: "../../xlights/include/xSchedule64.ico"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../bin/xScheduleWeb\*.*"; DestDir: "{app}/xScheduleWeb"; Flags: ignoreversion recursesubdirs
Source: "../../controllers\*.*"; DestDir: "{app}/controllers"; Flags: ignoreversion recursesubdirs

; xSMSDaemon
Source: "../../xSchedule/xSMSDaemon/x64/Release/xSMSDaemon.dll"; DestDir: "{app}"
Source: "../../xSchedule/xSMSDaemon/Blacklist.txt"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xSchedule/xSMSDaemon/Whitelist.txt"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xSchedule/xSMSDaemon/PhoneBlacklist.txt"; DestDir: "{app}"; Flags: "ignoreversion"

; RemoteFalcon
Source: "../../xSchedule/RemoteFalcon/x64/Release/RemoteFalcon.dll"; DestDir: "{app}"

; DLLs from the xLights dependency bundle (staged into xlights/bin64 by xlights/ci_scripts/fetch_dependencies.ps1)
Source: "../../xlights/bin64/libcurl.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/avcodec-62.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/avfilter-11.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/avformat-62.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/avutil-60.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/swresample-6.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/swscale-9.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/SDL2.dll"; DestDir: "{app}"; Flags: "ignoreversion"

; readmes and licenses
Source: "../../LICENSE"; DestDir: "{app}";

; VC++ Redistributable
Source: "../../xlights/build_scripts/msw/vcredist/VC_redist.x64.exe"; DestDir: {tmp}; DestName: "vc_redist.x64.exe"; Flags: deleteafterinstall

[Icons]
Name: "{group}\xSchedule"; Filename: "{app}\xSchedule.EXE"; WorkingDir: "{app}"
Name: "{commondesktop}\xSchedule"; Filename: "{app}\xSchedule.EXE"; WorkingDir: "{app}"; Tasks: desktopicon; IconFilename: "{app}\xSchedule64.ico";

[Run]
Filename: {tmp}\vc_redist.x64.exe; \
    Parameters: "/q /passive /norestart /Q:a /c:""msiexec /q /i vcredist.msi"""; \
    StatusMsg: "Installing VC++ Redistributables..."

Filename: "{app}\xSchedule.exe"; Description: "Launch application"; Flags: postinstall nowait skipifsilent

[Registry]
Root: HKCU; Subkey: "Software\xSchedule"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\xSMSDaemon"; Flags: uninsdeletekey

; -- xSchedule_64bit.iss --
; Installer for xSchedule standalone

#include "xSchedule_common.iss"

#define Bits 64

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
OutputBaseFilename={#MyTitleName}{#Bits}_{#Year}_{#Version}{#Other}

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "Do you want to create desktop icon?"; Flags: checkablealone

[Files]
; xSchedule
Source: "../../xSchedule/x64/Release/xSchedule.exe"; DestDir: "{app}"
Source: "../../xlights/include/xSchedule64.ico"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../bin/xScheduleWeb\*.*"; DestDir: "{app}/xScheduleWeb"; Flags: ignoreversion recursesubdirs

; xSMSDaemon
Source: "../../xSchedule/xSMSDaemon/x64/Release/xSMSDaemon.dll"; DestDir: "{app}"
Source: "../../xSchedule/xSMSDaemon/Blacklist.txt"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xSchedule/xSMSDaemon/Whitelist.txt"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xSchedule/xSMSDaemon/PhoneBlacklist.txt"; DestDir: "{app}"; Flags: "ignoreversion"

; RemoteFalcon
Source: "../../xSchedule/RemoteFalcon/x64/Release/RemoteFalcon.dll"; DestDir: "{app}"

; DLLs from xLights repo
Source: "../../xlights/bin64/libcurl-x64.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/avcodec-60.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/avformat-60.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/avutil-58.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/swresample-4.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/swscale-7.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/bin64/SDL2.dll"; DestDir: "{app}"; Flags: "ignoreversion"

; readmes and licenses
Source: "../../LICENSE"; DestDir: "{app}";

; VC++ Redistributable
Source: "vcredist/vc_redist.x64.exe"; DestDir: {tmp}; Flags: deleteafterinstall

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

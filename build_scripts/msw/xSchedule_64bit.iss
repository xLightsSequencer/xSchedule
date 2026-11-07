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
Source: "../../xlights/resources/controllers\*.*"; DestDir: "{app}/controllers"; Flags: ignoreversion recursesubdirs

; xSMSDaemon
Source: "../../xSchedule/xSMSDaemon/x64/Release/xSMSDaemon.dll"; DestDir: "{app}"
Source: "../../xSchedule/xSMSDaemon/Blacklist.txt"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xSchedule/xSMSDaemon/Whitelist.txt"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xSchedule/xSMSDaemon/PhoneBlacklist.txt"; DestDir: "{app}"; Flags: "ignoreversion"

; RemoteFalcon
Source: "../../xSchedule/RemoteFalcon/x64/Release/RemoteFalcon.dll"; DestDir: "{app}"

; DLLs from the xLights dependency bundle (fetched by build_xSchedule_x64.cmd)
Source: "../../xlights/dependencies-bundle/bin/libcurl.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/dependencies-bundle/bin/avcodec-62.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/dependencies-bundle/bin/avfilter-11.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/dependencies-bundle/bin/avformat-62.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/dependencies-bundle/bin/avutil-60.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/dependencies-bundle/bin/swresample-6.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/dependencies-bundle/bin/swscale-9.dll"; DestDir: "{app}"; Flags: "ignoreversion"
Source: "../../xlights/dependencies-bundle/bin/SDL2.dll"; DestDir: "{app}"; Flags: "ignoreversion"

; readmes and licenses
Source: "../../LICENSE"; DestDir: "{app}";
Source: "../../xSchedule/libltc/COPYING"; DestDir: "{app}/licenses"; DestName: "libltc-COPYING.txt"
Source: "../../xSchedule/portmidi/license.txt"; DestDir: "{app}/licenses"; DestName: "portmidi-license.txt"

; VC++ Redistributable (fetched by build_xSchedule_x64.cmd); must be at least as new as the build toolset
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
Root: HKCU; Subkey: "Software\RemoteFalcon"; Flags: uninsdeletekey

[UninstallDelete]
Type: filesandordirs; Name: "{userappdata}\xSchedule"

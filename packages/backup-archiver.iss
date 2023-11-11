[Setup]
AppName=Backup ARchiver
AppVerName=BAR @VERSION@
AppPublisher=Torsten Rupp
AppPublisherURL=http://www.kigen.de/projects/bar/index.html
AppSupportURL=http://www.kigen.de/projects/bar/index.html
AppUpdatesURL=http://www.kigen.de/projects/bar/index.html
DefaultDirName={pf}\BAR
DefaultGroupName=BAR
ShowLanguageDialog=yes
SetupLogging=yes

; debug only
Compression = none

[Tasks]
; NOTE: The following entry contains English phrases ("Create a desktop icon" and "Additional icons"). You are free to translate them into another language if required.
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"

[Files]
Source: "tmp\usr\bin\bar.exe";                   DestDir: "{app}";     Flags: ignoreversion
Source: "tmp\usr\bin\bar-debug.exe";             DestDir: "{app}";     Flags: ignoreversion
Source: "tmp\usr\bin\bar-index.exe";             DestDir: "{app}";     Flags: ignoreversion
Source: "tmp\usr\bin\bar-index-debug.exe";       DestDir: "{app}";     Flags: ignoreversion
Source: "tmp\usr\bin\bar-keygen.cmd";            DestDir: "{app}";     Flags: ignoreversion
Source: "tmp\usr\bin\libgcc_s_*.dll";            DestDir: "{app}";     Flags: ignoreversion
Source: "tmp\usr\bin\libstdc++-6.dll";           DestDir: "{app}";     Flags: ignoreversion
Source: "tmp\usr\bin\libwinpthread-1.dll";       DestDir: "{app}";     Flags: ignoreversion
Source: "tmp\usr\bin\libpq.dll";                 DestDir: "{app}";     Flags: ignoreversion
Source: "tmp\usr\bin\barcontrol.exe";            DestDir: "{app}";     Flags: ignoreversion
Source: "tmp\usr\bin\barcontrol-windows_64.jar"; DestDir: "{app}";     Flags: ignoreversion
Source: "tmp\usr\bin\jre\*";                     DestDir: "{app}\jre"; Flags: ignoreversion recursesubdirs

[Dirs]
Name: "{localappdata}\BAR\jobs"
; Note: why is {sys} here the 64bit path?
Name: "{sys}\config\systemprofile\AppData\Local\BAR"
Name: "{sys}\config\systemprofile\AppData\Local\BAR\jobs"

[Icons]
Name: "{group}\BARControl";       Filename: "{app}\barcontrol.exe"
Name: "{userdesktop}\BARControl"; Filename: "{app}\barcontrol.exe"; Tasks: desktopicon

[Run]
; create default configuration files
Filename: {app}\bar.exe; Parameters: "--no-default-config --save-configuration={win}\bar.cfg" ; Flags: runhidden
Filename: {app}\bar.exe; Parameters: "--no-default-config --save-configuration={syswow64}\config\systemprofile\AppData\Local\BAR\bar.cfg" ; Flags: runhidden
; create and start service
Filename: {sys}\sc.exe; Parameters: "create ""BAR Server"" binPath= ""\""{app}\bar.exe\"" --server --daemon"" start= auto" ; Flags: runhidden
Filename: {sys}\sc.exe; Parameters: "start ""BAR Server""" ; Flags: runhidden

[UninstallRun]
; stop and delete service
Filename: {sys}\sc.exe; Parameters: "stop ""BAR Server""" ;   Flags: runhidden
Filename: {sys}\sc.exe; Parameters: "delete ""BAR Server""" ; Flags: runhidden

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
; TODO: UTF8 do not work?
;Name: "de"; MessagesFile: "compiler:backup-archiver-de.isl"

[Messages]
BeveledLabel=Backup ARchiver Setup

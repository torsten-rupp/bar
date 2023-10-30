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

[Tasks]
; NOTE: The following entry contains English phrases ("Create a desktop icon" and "Additional icons"). You are free to translate them into another language if required.
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"

[Files]
Source: "tmp\usr\bin\bar.exe";                   DestDir: "{app}"; Flags: ignoreversion
Source: "tmp\usr\bin\bar-debug.exe";             DestDir: "{app}"; Flags: ignoreversion
Source: "tmp\usr\bin\bar-index.exe";             DestDir: "{app}"; Flags: ignoreversion
Source: "tmp\usr\bin\bar-index-debug.exe";       DestDir: "{app}"; Flags: ignoreversion
Source: "tmp\usr\bin\bar-keygen.cmd";            DestDir: "{app}"; Flags: ignoreversion
Source: "tmp\usr\bin\libgcc_s_*.dll";            DestDir: "{app}"; Flags: ignoreversion
Source: "tmp\usr\bin\libstdc++-6.dll";           DestDir: "{app}"; Flags: ignoreversion
Source: "tmp\usr\bin\libwinpthread-1.dll";       DestDir: "{app}"; Flags: ignoreversion
Source: "tmp\usr\bin\libpq.dll";                 DestDir: "{app}"; Flags: ignoreversion
Source: "tmp\etc\bar\bar.cfg";                   DestDir: "{win}"; Flags: ignoreversion
Source: "tmp\usr\bin\barcontrol.cmd";            DestDir: "{app}"; Flags: ignoreversion
Source: "tmp\usr\bin\barcontrol-windows_64.jar"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
Name: "{localappdata}\BAR\jobs"

[Icons]
Name: "{group}\BAR";       Filename: "{app}\bar.exe"
Name: "{userdesktop}\BAR"; Filename: "{app}\bar.exe"; Tasks: desktopicon

[Run]
; NOTE: The following entry contains an English phrase ("Launch"). You are free to translate it into another language if required.
;Filename: "{app}\bar.exe"; Description: "Launch BAR"; Flags: nowait postinstall skipifsilent

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
; TODO: UTF8 do not work?
;Name: "de"; MessagesFile: "compiler:backup-archiver-de.isl"

[Messages]
BeveledLabel=Backup ARchiver Setup

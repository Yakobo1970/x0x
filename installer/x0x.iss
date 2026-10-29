; x0x Setup - Inno Setup Skript
;
; Installiert das VST3-Plugin nach C:\Program Files\Common Files\VST3 und das
; Standalone-Programm nach C:\Program Files\x0x (beides waehlbar).
;
; Vorher Release bauen:
;   cmake --build build --config Release --target x0x_VST3 x0x_Standalone
; Dann:
;   "C:\Program Files\Inno Setup 7\ISCC.exe" installer\x0x.iss

#define AppName "x0x"
#define AppVersion "1.0.0"
#define AppPublisher "Kay 'Yakobo' Fiedler (Yakobo1970)"
#define AppURL "https://www.undabtanzbar.de/software/"
#define AppExeName "x0x.exe"
#define BuildDir "..\build\x0x_artefacts\Release"

[Setup]
AppId={{80390C1E-4B08-4A9E-B4D9-0ED6F3FD8BE1}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}
AppUpdatesURL=https://github.com/Yakobo1970
AppComments=Acid Bass & Rhythm Rack - VST3 und Standalone
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\{#AppExeName}
OutputDir=..\dist_installer
OutputBaseFilename=x0x-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
; Der VST3-Ordner unter Common Files braucht Administratorrechte.
PrivilegesRequired=admin
LicenseFile=..\LICENSE
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
SetupIconFile=..\assets\x0x.ico

[Languages]
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "VST3 + Standalone"
Name: "custom"; Description: "Benutzerdefiniert"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3-Plugin (C:\Program Files\Common Files\VST3)"; Types: full custom
Name: "standalone"; Description: "Standalone-Programm"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Components: standalone

[Files]
Source: "{#BuildDir}\VST3\x0x.vst3\*"; DestDir: "{commoncf64}\VST3\x0x.vst3"; Components: vst3; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#BuildDir}\Standalone\{#AppExeName}"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\MIDI_CC_MAP.md"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Components: standalone
Name: "{group}\MIDI-Belegung"; Filename: "{app}\MIDI_CC_MAP.md"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent; Components: standalone

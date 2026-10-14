; livelooper - Windows installer (Inno Setup 6)
; Built by CI:  iscc /DMyAppVersion=1.5.0 /O<output dir> installer\windows\livelooper.iss
; Installs the standalone app to Program Files\livelooper (with a Start menu shortcut) and the
; VST3 plug-in to Common Files\VST3. The ZIP download and the manual installation keep working
; as an alternative.

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#define MyAppName "livelooper"
#define Root "..\.."
#define Artefacts Root + "\build\livelooper_artefacts\Release"

[Setup]
AppId={{6F1C2A7E-3B9D-4C55-9E21-5A8B7C0D4E31}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher=groovex
AppPublisherURL=https://github.com/thegroovex/livelooper
AppSupportURL=https://github.com/thegroovex/livelooper/issues
AppUpdatesURL=https://github.com/thegroovex/livelooper/releases
DefaultDirName={autopf}\livelooper
DefaultGroupName=livelooper
DisableProgramGroupPage=yes
LicenseFile={#Root}\LICENSE
OutputBaseFilename=livelooper-Windows-Setup
SetupIconFile=livelooper.ico
UninstallDisplayIcon={app}\livelooper.exe
UninstallDisplayName={#MyAppName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
CloseApplications=yes
VersionInfoVersion={#MyAppVersion}
VersionInfoCompany=groovex
VersionInfoDescription=livelooper installer

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "de"; MessagesFile: "compiler:Languages\German.isl"

[CustomMessages]
en.CompStandalone=livelooper app (standalone)
de.CompStandalone=livelooper-App (Standalone)
en.CompVst3=VST3 plug-in (Common Files\VST3)
de.CompVst3=VST3-Plug-in (Gemeinsame Dateien\VST3)
en.DesktopIcon=Create a desktop shortcut
de.DesktopIcon=Verknüpfung auf dem Desktop anlegen
en.LaunchApp=Start livelooper now
de.LaunchApp=livelooper jetzt starten
en.UserGuide=livelooper user guide
de.UserGuide=livelooper Bedienungsanleitung
en.TypeFull=Full (app and VST3)
de.TypeFull=Vollständig (App und VST3)
en.TypeCustom=Custom
de.TypeCustom=Benutzerdefiniert

[Types]
Name: "full"; Description: "{cm:TypeFull}"
Name: "custom"; Description: "{cm:TypeCustom}"; Flags: iscustom

[Components]
Name: "app"; Description: "{cm:CompStandalone}"; Types: full custom
Name: "vst3"; Description: "{cm:CompVst3}"; Types: full custom

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopIcon}"; Components: app; Flags: unchecked

[Files]
Source: "{#Artefacts}\Standalone\livelooper.exe"; DestDir: "{app}"; Components: app; Flags: ignoreversion
Source: "{#Root}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "{#Root}\THIRD-PARTY.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\docs\en\INSTALLATION.md"; DestDir: "{app}"; DestName: "INSTALLATION.md"; Flags: ignoreversion
Source: "{#Root}\docs\de\INSTALLATION.md"; DestDir: "{app}"; DestName: "INSTALLATION-DE.md"; Flags: ignoreversion
Source: "{#Artefacts}\VST3\livelooper.vst3\*"; DestDir: "{commoncf64}\VST3\livelooper.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\livelooper"; Filename: "{app}\livelooper.exe"; Components: app
Name: "{group}\{cm:UserGuide}"; Filename: "https://github.com/thegroovex/livelooper/blob/main/docs/en/USER-GUIDE.md"; Languages: en
Name: "{group}\{cm:UserGuide}"; Filename: "https://github.com/thegroovex/livelooper/blob/main/docs/de/BEDIENUNGSANLEITUNG.md"; Languages: de
Name: "{group}\{cm:UninstallProgram,livelooper}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\livelooper"; Filename: "{app}\livelooper.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\livelooper.exe"; Description: "{cm:LaunchApp}"; Components: app; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\livelooper.vst3"

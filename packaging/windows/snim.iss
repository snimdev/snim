; Build with: ISCC /DAppVersion=1.0.0-alpha.1 /DAppVersionNumeric=1.0.0 /DSourceDir=<install prefix> /O<outdir> snim.iss

#ifndef AppVersion
  #error Pass /DAppVersion=<version>
#endif
#ifndef AppVersionNumeric
  #error Pass /DAppVersionNumeric=<X.Y.Z>
#endif
#ifndef SourceDir
  #error Pass /DSourceDir=<cmake --install prefix>
#endif

[Setup]
; Never change the AppId: it is how upgrades and the uninstaller find an existing install.
AppId={{6BF06980-5D1A-42D7-AD5D-67A4640D9701}
AppName=Snim
AppVersion={#AppVersion}
AppPublisher=darkog
AppPublisherURL=https://snim.dev
AppSupportURL=https://snim.dev
AppUpdatesURL=https://snim.dev/download
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
DefaultDirName={autopf}\Snim
DisableProgramGroupPage=yes
; Created by snim.exe at startup, so setup and uninstall wait until Snim is closed.
AppMutex=Local\dev.snim.Snim
UninstallDisplayName=Snim
UninstallDisplayIcon={app}\snim.exe
SetupIconFile=..\..\resources\snim.ico
LicenseFile=..\..\LICENSE
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.19041
WizardStyle=modern
OutputBaseFilename=Snim-{#AppVersion}-windows-x64-setup
VersionInfoVersion={#AppVersionNumeric}
VersionInfoProductTextVersion={#AppVersion}
Compression=lzma2/max
SolidCompression=yes

[Tasks]
Name: desktopicon; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: startup; Description: "Start Snim when you sign in"; GroupDescription: "Startup:"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Snim"; Filename: "{app}\snim.exe"; AppUserModelID: "dev.snim.Snim"
Name: "{autodesktop}\Snim"; Filename: "{app}\snim.exe"; AppUserModelID: "dev.snim.Snim"; Tasks: desktopicon

[Registry]
; HKA is HKCU for a per-user install and HKLM for an all-users one.
Root: HKA; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "Snim"; ValueData: """{app}\snim.exe"""; Flags: uninsdeletevalue; Tasks: startup

[Run]
Filename: "{app}\snim.exe"; Description: "{cm:LaunchProgram,Snim}"; Flags: nowait postinstall skipifsilent

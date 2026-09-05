#ifndef SourceDir
  #error SourceDir must identify the staged source-inspector-dock folder.
#endif
#ifndef PackageVersion
  #error PackageVersion is required.
#endif
#ifndef OutputDir
  #error OutputDir is required.
#endif
#ifndef SignedBuild
  #define SignedBuild 1
#endif

[Setup]
AppId={{498D04E9-B0DD-48AF-BD69-70198B78EF3F}
AppName=Source Inspector Dock
AppVersion={#PackageVersion}
AppPublisher=MITCHELL SOFTWARE SOLUTIONS LLC
AppPublisherURL=https://github.com/prgmitchell/source-inspector-dock
AppSupportURL=https://github.com/prgmitchell/source-inspector-dock/issues
DefaultDirName={commonappdata}\obs-studio\plugins\source-inspector-dock
DisableDirPage=yes
DisableProgramGroupPage=yes
UsePreviousAppDir=no
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
OutputDir={#OutputDir}
#if Int(SignedBuild)
OutputBaseFilename=source-inspector-dock-{#PackageVersion}-windows-x64-setup
SignTool=artifact
SignedUninstaller=yes
SignedUninstallerDir={#OutputDir}\signed-uninstaller
SignToolRetryCount=2
SignToolRetryDelay=3000
#else
OutputBaseFilename=source-inspector-dock-{#PackageVersion}-windows-x64-setup-unsigned
SignedUninstaller=no
#endif
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName=Source Inspector Dock for OBS Studio
VersionInfoVersion={#PackageVersion}.0
VersionInfoCompany=MITCHELL SOFTWARE SOLUTIONS LLC
VersionInfoDescription=Source Inspector Dock for OBS Studio
VersionInfoProductName=Source Inspector Dock
LicenseFile={#SourceDir}\LICENSE
CloseApplications=yes
CloseApplicationsFilter=source-inspector-dock.dll
RestartApplications=no

[Files]
Source: "{#SourceDir}\bin\64bit\source-inspector-dock.dll"; DestDir: "{app}\bin\64bit"; Flags: ignoreversion
Source: "{#SourceDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#SourceDir}\README.md"; DestDir: "{app}"; Flags: ignoreversion

[Messages]
WelcomeLabel2=This installs Source Inspector Dock for OBS Studio.%n%nClose OBS before continuing. After installation, start OBS and enable Docks > Source Inspector.
FinishedLabel=Source Inspector Dock is installed.%n%nStart OBS and enable Docks > Source Inspector. Select a source to inspect its properties, transform, and filters.

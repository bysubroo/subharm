; SubHarm Windows installer (Inno Setup 6): VST3 plugin + standalone app.
; Built by CI:  ISCC /DAppVersion=x.y.z /DVst3Dir=<SubHarm.vst3> /DAppExe=<SubHarm.exe> /O<outdir> SubHarm.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

[Setup]
AppId={{8E6B2E4A-5B1F-4C3E-9A77-3D2B6F0C1A11}
AppName=SubHarm
AppVersion={#AppVersion}
AppVerName=SubHarm {#AppVersion}
AppPublisher=SubRoo
AppPublisherURL=https://subroo.xyz
AppComments=Chord-aware vocal harmony stacks (VST3 + standalone)
DefaultDirName={commonpf64}\SubRoo\SubHarm
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName=SubHarm
UninstallDisplayIcon={app}\SubHarm.exe
OutputBaseFilename=SubHarm-{#AppVersion}-Windows-Setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
CloseApplications=yes

[Tasks]
Name: "desktopicon"; Description: "Desktop shortcut for the SubHarm app"; Flags: unchecked

[InstallDelete]
; replace an older plugin bundle completely
Type: filesandordirs; Name: "{commoncf64}\VST3\SubHarm.vst3"

[Files]
Source: "{#Vst3Dir}\*"; DestDir: "{commoncf64}\VST3\SubHarm.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
Source: "{#AppExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{commonprograms}\SubHarm"; Filename: "{app}\SubHarm.exe"
Name: "{commondesktop}\SubHarm"; Filename: "{app}\SubHarm.exe"; Tasks: desktopicon

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\SubHarm.vst3"

[Messages]
FinishedLabel=SubHarm is installed:%n  - VST3 plugin in the shared VST3 folder%n  - SubHarm app in the Start menu%n%nRescan plugins in your DAW:%n  FL Studio: Options > Manage plugins > Find more plugins%n  Cubase: Studio > VST Plug-in Manager > Rescan

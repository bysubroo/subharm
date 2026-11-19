; SubHarm Windows installer (Inno Setup 6)
; Built by CI:  ISCC /DAppVersion=x.y.z /DSrcDir=<path to SubHarm.vst3> /O<outdir> SubHarm.iss

#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef SrcDir
  #define SrcDir "..\..\build\src\plugin\SubHarm_artefacts\Release\VST3\SubHarm.vst3"
#endif

[Setup]
AppId={{8E6B2E4A-5B1F-4C3E-9A77-3D2B6F0C1A11}
AppName=SubHarm
AppVersion={#AppVersion}
AppVerName=SubHarm {#AppVersion}
AppPublisher=SubRoo
AppPublisherURL=https://subroo.xyz
AppComments=Chord-aware vocal harmony stacks (VST3)
DefaultDirName={commoncf64}\VST3\SubHarm.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
; keep the uninstaller out of the plugin bundle
UninstallFilesDir={commonpf64}\SubRoo\SubHarm
UninstallDisplayName=SubHarm (VST3)
OutputBaseFilename=SubHarm-{#AppVersion}-Windows-Setup
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
CloseApplications=yes

[Files]
Source: "{#SrcDir}\*"; DestDir: "{app}"; Flags: recursesubdirs createallsubdirs ignoreversion

[InstallDelete]
; clean an older version of the bundle before copying the new one
Type: filesandordirs; Name: "{app}"

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

[Messages]
FinishedLabel=SubHarm is installed in the shared VST3 folder.%n%nRescan plugins in your DAW:%n  FL Studio: Options > Manage plugins > Find more plugins%n  Cubase: Studio > VST Plug-in Manager > Rescan

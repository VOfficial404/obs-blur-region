; Windows installer for obs-blur-region, compiled by .github/scripts/Package-Windows.ps1:
;   ISCC.exe /DMyVersion=<version> /DMySource=<release/<config>/obs-blur-region> /DMyOutputDir=<dir> /DMyOutputName=<name> installer.iss
; ChineseSimplified.isl is the user-contributed translation from jrsoftware/issrc (is-6_7_1, Files/Languages/Unofficial),
; saved with a UTF-8 BOM so ISCC reads it as Unicode.

#ifndef MyVersion
  #error MyVersion is required
#endif

[Setup]
; Fixed for the life of the project: Windows matches upgrades and the uninstaller by it.
AppId={{8046D4E5-5904-41EC-90E3-BE956251A7A6}
AppName=Blur Region for OBS
AppVersion={#MyVersion}
AppVerName=Blur Region for OBS {#MyVersion}
AppPublisher=VOfficial404
AppPublisherURL=https://github.com/VOfficial404/obs-blur-region
AppSupportURL=https://github.com/VOfficial404/obs-blur-region/issues
AppUpdatesURL=https://github.com/VOfficial404/obs-blur-region/releases/latest
; Where OBS 31+ (website and Steam builds) look for third-party plugins. Portable OBS users pick
; <OBS folder>\plugins\obs-blur-region on the directory page instead.
DefaultDirName={commonappdata}\obs-studio\plugins\obs-blur-region
UsePreviousAppDir=yes
DisableDirPage=auto
DirExistsWarning=no
DisableProgramGroupPage=yes
DisableReadyPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; OBS keeps the plugin DLL open; Restart Manager finds obs64.exe through it and offers to close it.
CloseApplications=yes
RestartApplications=no
UninstallDisplayName=Blur Region for OBS
UninstallFilesDir={app}\uninstall
OutputDir={#MyOutputDir}
OutputBaseFilename={#MyOutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "chinesesimplified"; MessagesFile: "ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Messages]
chinesesimplified.FinishedLabel=Blur Region for OBS 已安装。启动（或重启）OBS 后，在场景的「来源」里点「+」，添加「模糊区域」。
english.FinishedLabel=Blur Region for OBS is installed. Start (or restart) OBS, then add a "Blur Region" source to your scene with the + button under Sources.

[Files]
Source: "{#MySource}\*"; DestDir: "{app}"; Excludes: "*.pdb"; Flags: ignoreversion recursesubdirs createallsubdirs

[Run]
; ProgramData lets every user create files in new folders, and OBS loads DLLs from this one for whoever starts it.
; An earlier install or another user may own files here: hand everything to Administrators and reset child ACLs to
; inherit-only, so the grant below reaches every file rather than just the folder.
Filename: "{sys}\icacls.exe"; Parameters: """{app}"" /setowner *S-1-5-32-544 /T /C"; Flags: runhidden waituntilterminated
Filename: "{sys}\icacls.exe"; Parameters: """{app}"" /reset /T /C"; Flags: runhidden waituntilterminated
; Drop the inherited ACL: only Administrators and SYSTEM can write, Users can only read and run.
Filename: "{sys}\icacls.exe"; Parameters: """{app}"" /inheritance:r /grant:r *S-1-5-32-544:(OI)(CI)F /grant:r *S-1-5-18:(OI)(CI)F /grant:r *S-1-5-32-545:(OI)(CI)RX"; Flags: runhidden waituntilterminated

#define DOSBoxAppName "DOSBox Staging"
#define DOSBoxAppDirName "DOSBox Staging"
#define DOSBoxAppVersion "DOSBOX-STAGING-VERSION"
#define DOSBoxAppInternal "dosbox-staging"
#define DOSBoxAppURL "https://dosbox-staging.github.io/"
#define DOSBoxAppExeName "dosbox.exe"
#define DOSBoxAppExeDebuggerName "dosbox_with_debugger.exe"
#define DOSBoxAppExeMSVCName "dosbox_msvc.exe"
#define DOSBoxAppExeMSVCDebuggerName "dosbox_msvc_with_debugger.exe"
#define DOSBoxAppExeConsoleName "dosbox_with_console.bat"
#define DOSBoxAppBuildDate GetDateTimeString('yyyymmdd_hhnnss', '', '')

[Setup]
AppId={{587471B7-02F6-4F25-8D00-70006790653C}
AppName={#DOSBoxAppName}
AppVersion={#DOSBoxAppVersion}
AppVerName={#DOSBoxAppName} {#DOSBoxAppVersion}
AppPublisherURL={#DOSBoxAppURL}
AppSupportURL={#DOSBoxAppURL}
AppUpdatesURL={#DOSBoxAppURL}
DefaultDirName={autopf}\{#DOSBoxAppDirName}
DefaultGroupName={#DOSBoxAppName}
DisableDirPage=no
DisableWelcomePage=no
DisableProgramGroupPage=yes
InfoBeforeFile=setup_preamble.txt
OutputDir=.\
OutputBaseFilename={#DOSBoxAppInternal}-{#DOSBoxAppVersion}-setup
SetupIconFile={#DOSBoxAppInternal}.ico
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline dialog
UninstallDisplayIcon={app}\{#DOSBoxAppExeName}
WizardSmallImageFile={#DOSBoxAppInternal}.bmp
WizardImageFile={#DOSBoxAppInternal}-side.bmp

[Messages]
InfoBeforeLabel=Please read the general information about {#DOSBoxAppName} below.

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "armenian"; MessagesFile: "compiler:Languages\Armenian.isl"
Name: "bulgarian"; MessagesFile: "compiler:Languages\Bulgarian.isl"
Name: "catalan"; MessagesFile: "compiler:Languages\Catalan.isl"
Name: "corsican"; MessagesFile: "compiler:Languages\Corsican.isl"
Name: "czech"; MessagesFile: "compiler:Languages\Czech.isl"
Name: "danish"; MessagesFile: "compiler:Languages\Danish.isl"
Name: "dutch"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "finnish"; MessagesFile: "compiler:Languages\Finnish.isl"
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"
Name: "hebrew"; MessagesFile: "compiler:Languages\Hebrew.isl"
Name: "icelandic"; MessagesFile: "compiler:Languages\Icelandic.isl"
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl"
Name: "japanese"; MessagesFile: "compiler:Languages\Japanese.isl"
Name: "norwegian"; MessagesFile: "compiler:Languages\Norwegian.isl"
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"
Name: "portuguese"; MessagesFile: "compiler:Languages\Portuguese.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "slovak"; MessagesFile: "compiler:Languages\Slovak.isl"
Name: "slovenian"; MessagesFile: "compiler:Languages\Slovenian.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "turkish"; MessagesFile: "compiler:Languages\Turkish.isl"
Name: "ukrainian"; MessagesFile: "compiler:Languages\Ukrainian.isl"

[Tasks]
Name: "contextmenu"; Description: "Add ""Run/Open with {#DOSBoxAppName}"" context menu for Windows Explorer"
Name: "defaultmsvc"; Description: "Run MSVC release binary (instead of MSYS2 binary) as the default binary"; Flags: unchecked
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"

[Run]
Filename: "{app}\{#DOSBoxAppExeName}"; Parameters: "-c ""config -wc"" -noconsole"; Description: "{cm:LaunchProgram,{#StringChange(DOSBoxAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Files]
Source: "program\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#DOSBoxAppExeName}"; DestDir: "{app}"; DestName: "{#DOSBoxAppExeName}"; Flags: ignoreversion; Tasks: not defaultmsvc
Source: "{#DOSBoxAppExeDebuggerName}"; DestDir: "{app}"; DestName: "{#DOSBoxAppExeDebuggerName}"; Flags: ignoreversion; Tasks: not defaultmsvc
Source: "{#DOSBoxAppExeMSVCName}"; DestDir: "{app}"; DestName: "{#DOSBoxAppExeName}"; Flags: ignoreversion; Tasks: defaultmsvc
Source: "{#DOSBoxAppExeMSVCDebuggerName}"; DestDir: "{app}"; DestName: "{#DOSBoxAppExeDebuggerName}"; Flags: ignoreversion; Tasks: defaultmsvc
Source: "{#DOSBoxAppExeConsoleName}"; DestDir: "{app}"; DestName: "{#DOSBoxAppExeConsoleName}"; Flags: ignoreversion
Source: "{#DOSBoxAppInternal}.ico"; DestDir: "{app}"; DestName: "{#DOSBoxAppInternal}.ico"; Flags: ignoreversion

[Icons]
Name: "{group}\{#DOSBoxAppName}"; Filename: "{app}\{#DOSBoxAppExeName}"; Parameters: "-noconsole"
Name: "{group}\{#DOSBoxAppName} with console"; Filename: "{app}\{#DOSBoxAppExeConsoleName}"; IconFilename: "{app}\{#DOSBoxAppInternal}.ico"
Name: "{group}\{#DOSBoxAppName} with debugger"; Filename: "{app}\{#DOSBoxAppExeDebuggerName}"
Name: "{group}\Visit {#DOSBoxAppName} website"; Filename: "{#DOSBoxAppURL}"
Name: "{autodesktop}\{#DOSBoxAppName}"; Filename: "{app}\{#DOSBoxAppExeName}"; Parameters: "-noconsole"; Tasks: desktopicon

[Registry]
Root: HKA; Subkey: "Software\{#DOSBoxAppInternal}"; Flags: uninsdeletekeyifempty
Root: HKA; Subkey: "Software\{#DOSBoxAppInternal}"; ValueType: string; ValueName: "Path"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\{#DOSBoxAppInternal}"; ValueType: string; ValueName: "Version"; ValueData: "{#DOSBoxAppVersion}"; Flags: uninsdeletekey

; Add context menu entry when right-clicking on a folder
Root: HKA; Subkey: "Software\Classes\Directory\shell\{#DOSBoxAppInternal}"; ValueType: string; ValueName: ""; ValueData: "Open with {#DOSBoxAppName}"; Check: WizardIsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Directory\shell\{#DOSBoxAppInternal}"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\{#DOSBoxAppExeConsoleName}"",0"; Check: WizardIsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Directory\shell\{#DOSBoxAppInternal}\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#DOSBoxAppExeConsoleName}"" --working-dir ""%v"""; Check: WizardIsTaskSelected('contextmenu'); Flags: uninsdeletekey

Root: HKA; Subkey: "Software\Classes\Directory\shell\{#DOSBoxAppInternal}"; ValueType: none; Check: not WizardIsTaskSelected('contextmenu'); Flags: deletekey
Root: HKA; Subkey: "Software\Classes\Directory\shell\{#DOSBoxAppInternal}\command"; ValueType: none; Check: not WizardIsTaskSelected('contextmenu'); Flags: deletekey

; Add context menu when right-clicking on an empty area within a folder
Root: HKA; Subkey: "Software\Classes\Directory\Background\shell\{#DOSBoxAppInternal}"; ValueType: string; ValueName: ""; ValueData: "Open with {#DOSBoxAppName}"; Check: WizardIsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\Directory\Background\shell\{#DOSBoxAppInternal}"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\{#DOSBoxAppExeConsoleName}"",0"; Check: WizardIsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Directory\Background\shell\{#DOSBoxAppInternal}\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#DOSBoxAppExeConsoleName}"" --working-dir ""%v"""; Check: WizardIsTaskSelected('contextmenu'); Flags: uninsdeletekey

Root: HKA; Subkey: "Software\Classes\Directory\Background\shell\{#DOSBoxAppInternal}"; ValueType: none; Check: not WizardIsTaskSelected('contextmenu'); Flags: deletekey
Root: HKA; Subkey: "Software\Classes\Directory\Background\shell\{#DOSBoxAppInternal}\command"; ValueType: none; Check: not WizardIsTaskSelected('contextmenu'); Flags: deletekey

; Add context menu when right-clicking on `.conf` extension
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.conf\shell\Open with {#DOSBoxAppName}"; ValueType: none; ValueName: ""; ValueData: ""; Check: WizardIsTaskSelected('contextmenu'); Flags: uninsdeletevalue
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.conf\shell\Open with {#DOSBoxAppName}"; ValueType: string; ValueName: "Icon"; ValueData: """{app}\{#DOSBoxAppExeConsoleName}"",0"; Check: WizardIsTaskSelected('contextmenu'); Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.conf\shell\Open with {#DOSBoxAppName}\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#DOSBoxAppExeConsoleName}"" -conf ""%1"""; Check: WizardIsTaskSelected('contextmenu'); Flags: uninsdeletekey

Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.conf\shell\Open with {#DOSBoxAppName}"; ValueType: none; Check: not WizardIsTaskSelected('contextmenu'); Flags: deletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.conf\shell\Open with {#DOSBoxAppName}\command"; ValueType: none; Check: not WizardIsTaskSelected('contextmenu'); Flags: deletekey

; Delete context menu entries for .exe, .com & .bat files potentially created by a previous installation
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.exe\shell\Run with {#DOSBoxAppName}"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.exe\shell\Run with {#DOSBoxAppName}\command"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.com\shell\Run with {#DOSBoxAppName}"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.com\shell\Run with {#DOSBoxAppName}\command"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.bat\shell\Run with {#DOSBoxAppName}"; ValueType: none; Flags: deletekey
Root: HKA; Subkey: "Software\Classes\SystemFileAssociations\.bat\shell\Run with {#DOSBoxAppName}\command"; ValueType: none; Flags: deletekey

[UninstallDelete]
Type: files; Name: "{app}\stderr.txt"
Type: files; Name: "{app}\stdout.txt"

[Code]
procedure CurPageChanged(CurPageID: Integer);
begin
  if CurPageID = wpFinished then
    WizardForm.RunList.Visible := False;
end;


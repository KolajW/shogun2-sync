#define MyAppName "Shogun-Sync"
#define MyAppVersion "0.1.2-beta"
#define MyAppPublisher "KolajW"
#define MyAppURL "https://shogun2-sync.com"
#define MyAppExeName "ShogunSync.exe"

[Setup]
AppId={{D37F8E3C-4321-4EBA-9D9A-2A478EFD6209}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={localappdata}\ShogunSync
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputBaseFilename=ShogunSync-Setup
OutputDir=dist_setup
SetupIconFile=icon.ico
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
CloseApplications=yes
CloseApplicationsFilter=shogun-sync.exe,ShogunSync.exe,Shogun2.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "startmenuicon"; Description: "Create a Start Menu shortcut"; GroupDescription: "{cm:AdditionalIcons}"
Name: "addtopath"; Description: "Add ShogunSync to user PATH environment variable"; GroupDescription: "System Integration:"

[Files]
Source: "shogun-sync.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "ShogunSync.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "shogun_overlay32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "libwinpthread-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "ShogunSync.cer"; DestDir: "{app}"; Flags: ignoreversion
Source: "icon.ico"; DestDir: "{app}"; Flags: ignoreversion

[Dirs]
Name: "{app}\saves"

[Icons]
Name: "{autodesktop}\Shogun Sync"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon; WorkingDir: "{app}"; IconFilename: "{app}\icon.ico"
Name: "{autoprograms}\Shogun Sync"; Filename: "{app}\{#MyAppExeName}"; Tasks: startmenuicon; WorkingDir: "{app}"; IconFilename: "{app}\icon.ico"

[Registry]
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Tasks: addtopath; Check: NeedsAddPath(ExpandConstant('{app}'))

[Run]
Filename: "certutil.exe"; Parameters: "-addstore -user -f ""TrustedPublisher"" ""{app}\ShogunSync.cer"""; Flags: runhidden
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + UpperCase(Param) + ';', ';' + UpperCase(OrigPath) + ';') = 0;
end;

function IsAppRunning(const FileName: string): Boolean;
var
  FSWbemLocator: Variant;
  FWMIService: Variant;
  FWbemObjectSet: Variant;
begin
  Result := False;
  try
    FSWbemLocator := CreateOleObject('WBEMScripting.SWBEMLocator');
    FWMIService := FSWbemLocator.ConnectServer('', 'root\CIMV2');
    FWbemObjectSet := FWMIService.ExecQuery(Format('SELECT Name FROM Win32_Process WHERE Name = "%s"', [FileName]));
    Result := (FWbemObjectSet.Count > 0);
  except
    Result := False;
  end;
end;

function PrepareSetup(var NeedsRestart: Boolean): Boolean;
var
  Ret: Integer;
begin
  Result := True;
  if WizardSilent then Exit;
  while IsAppRunning('shogun-sync.exe') or IsAppRunning('ShogunSync.exe') or IsAppRunning('Shogun2.exe') do
  begin
    Ret := MsgBox('Total War: Shogun 2 or Shogun Sync is currently running. Please close it before continuing with the installation.', mbConfirmation, MB_OKCANCEL);
    if Ret = IDCANCEL then
    begin
      Result := False;
      Exit;
    end;
  end;
end;

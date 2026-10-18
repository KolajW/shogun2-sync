#define MyAppName "Shogun-Sync"
#define MyAppVersion "0.1.3-beta"
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
CloseApplicationsFilter=shogun-sync.exe,ShogunSync.exe

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "startmenuicon"; Description: "Create a Start Menu shortcut"; GroupDescription: "{cm:AdditionalIcons}"
Name: "addtopath"; Description: "Add ShogunSync to user PATH environment variable"; GroupDescription: "System Integration:"

[Files]
Source: "shogun-sync.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "ShogunSync.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "shogun_overlay32.dll"; DestDir: "{app}"; Flags: ignoreversion restartreplace uninsrestartdelete
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
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent; WorkingDir: "{app}"

[UninstallDelete]
Type: filesandordirs; Name: "{app}"

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

procedure RemovePath(Param: string);
var
  OrigPath, NewPath, AppPath: string;
  P, L: Integer;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', OrigPath) then Exit;
  AppPath := UpperCase(Param);
  OrigPath := ';' + OrigPath + ';';
  P := Pos(';' + AppPath + ';', UpperCase(OrigPath));
  if P > 0 then
  begin
    L := Length(AppPath) + 1;
    Delete(OrigPath, P, L);
    NewPath := Copy(OrigPath, 2, Length(OrigPath) - 2);
    RegWriteStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', NewPath);
  end;
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
  AppDir, TempDll: string;
begin
  Result := True;
  Exec('taskkill.exe', '/f /im shogun-sync.exe /im ShogunSync.exe', '', SW_HIDE, ewWaitUntilTerminated, Ret);
  Sleep(200);
  AppDir := ExpandConstant('{localappdata}\ShogunSync');
  if FileExists(AppDir + '\shogun_overlay32.dll') then
  begin
    TempDll := AppDir + '\shogun_overlay32_' + GetDateTimeString('yyyymmddhhnnss', #0, #0) + '.old';
    if RenameFile(AppDir + '\shogun_overlay32.dll', TempDll) then
    begin
      RestartReplace(TempDll, '');
    end;
  end;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Ret: Integer;
  AppDir, TempDll: string;
begin
  if CurUninstallStep = usUninstall then
  begin
    Exec('taskkill.exe', '/f /im shogun-sync.exe /im ShogunSync.exe', '', SW_HIDE, ewWaitUntilTerminated, Ret);
    Sleep(200);
    RemovePath(ExpandConstant('{app}'));
    RegDeleteKeyIncludingSubkeys(HKEY_CURRENT_USER, 'Software\Microsoft\Windows\CurrentVersion\Uninstall\ShogunSync');
    AppDir := ExpandConstant('{app}');
    TempDll := AppDir + '\shogun_overlay32_' + GetDateTimeString('yyyymmddhhnnss', #0, #0) + '.old';
    if FileExists(AppDir + '\shogun_overlay32.dll') then
    begin
      if RenameFile(AppDir + '\shogun_overlay32.dll', TempDll) then
      begin
        RestartReplace(TempDll, '');
      end;
    end;
  end;
end;

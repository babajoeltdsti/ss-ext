; Inno Setup script for SS-EXT C++
#define AppName "SS-EXT"
#define AppVersion "0.1.0"
#define AppPublisher "OMERBABACO"
#define AppExeName "ss-ext.exe"
#define UpdaterExeName "ss-ext-updater.exe"

[Setup]
AppId={{3F963A36-3E59-4C24-9D59-9A3DD808E2C1}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\SS-EXT
DefaultGroupName=SS-EXT
OutputDir=..\dist
OutputBaseFilename=ss-ext-setup
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=lowest

[Files]
Source: "..\build\Release\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Release\{#UpdaterExeName}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\SS-EXT"; Filename: "{app}\{#AppExeName}"
Name: "{group}\SS-EXT (Stop)"; Filename: "{app}\{#AppExeName}"; Parameters: "--stop"
Name: "{commondesktop}\SS-EXT"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Masaustu kisayolu olustur"; GroupDescription: "Ek gorevler:"; Flags: unchecked

[Run]
Filename: "{app}\{#AppExeName}"; Description: "SS-EXT baslat"; Flags: nowait postinstall skipifsilent

[Code]
function ValidateInstalledBinaries(): Boolean;
begin
	Result := FileExists(ExpandConstant('{app}\{#AppExeName}')) and
						FileExists(ExpandConstant('{app}\{#UpdaterExeName}'));
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
	if CurStep = ssPostInstall then
	begin
		if not ValidateInstalledBinaries() then
		begin
			MsgBox('Kurulum dogrulamasi basarisiz: gerekli dosyalar eksik.', mbError, MB_OK);
			RaiseException('Carex-Ext installer validation failed.');
		end;
	end;
end;

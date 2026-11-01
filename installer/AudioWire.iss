; AudioWire developer/release installer. Unsigned builds are explicitly pre-alpha.
#ifndef AppVersion
#define AppVersion "0.1.0"
#endif
#ifndef Arch
#define Arch "x64"
#endif
#ifndef AppDir
#define AppDir "..\build\app\AudioWire_artefacts\Release"
#endif
#ifndef HelperDir
#define HelperDir "..\build\app"
#endif
#ifndef LicenseDir
#define LicenseDir "..\build\app\licenses"
#endif
#ifndef DriverDir
#define DriverDir "..\build\driver\x64\Release"
#endif
#if Ver < EncodeVer(6, 3, 0)
#define AllowedArch Arch
#else
#define AllowedArch Arch + "compatible"
#endif
[Setup]
AppId={{8A509E6C-42E1-4B19-9ECB-74B163143151}
AppName=AudioWire
AppVersion={#AppVersion}
AppPublisher=AudioWire contributors
AppPublisherURL=https://github.com/bryankruman/AudioWire
DefaultDirName={autopf}\AudioWire
DefaultGroupName=AudioWire
OutputDir=..\build\installer
OutputBaseFilename=AudioWire-{#AppVersion}-{#Arch}-Setup
LicenseFile=..\LICENSE
ArchitecturesAllowed={#AllowedArch}
ArchitecturesInstallIn64BitMode={#AllowedArch}
MinVersion=10.0.19041
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
CloseApplications=yes
RestartApplications=no
UninstallDisplayIcon={app}\AudioWire.exe
[Files]
Source: "{#AppDir}\AudioWire.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#HelperDir}\AudioWireDriverSetup.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#LicenseDir}\*"; DestDir: "{app}\licenses"; Flags: recursesubdirs createallsubdirs
Source: "..\LICENSE"; DestDir: "{app}"
Source: "..\LICENSE_EXCEPTION"; DestDir: "{app}"
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"
#ifdef WithDriver
Source: "{#DriverDir}\audiowire.inf"; DestDir: "{app}\driver"; Flags: ignoreversion
Source: "{#DriverDir}\audiowire.sys"; DestDir: "{app}\driver"; Flags: ignoreversion
Source: "{#DriverDir}\audiowire.cat"; DestDir: "{app}\driver"; Flags: ignoreversion
#endif
[Icons]
Name: "{group}\AudioWire"; Filename: "{app}\AudioWire.exe"
Name: "{group}\Uninstall AudioWire"; Filename: "{uninstallexe}"
[Registry]
Root: HKCR; Subkey: "audiowire"; ValueType: string; ValueData: "URL:AudioWire pairing"; Flags: uninsdeletekey
Root: HKCR; Subkey: "audiowire"; ValueType: string; ValueName: "URL Protocol"; ValueData: ""
Root: HKCR; Subkey: "audiowire\shell\open\command"; ValueType: string; ValueData: """{app}\AudioWire.exe"" ""%1"""
[Run]
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""AudioWire audio"" dir=in action=allow program=""{app}\AudioWire.exe"" enable=yes profile=private protocol=UDP remoteip=LocalSubnet"; Flags: runhidden waituntilterminated
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""AudioWire pairing"" dir=in action=allow program=""{app}\AudioWire.exe"" enable=yes profile=private protocol=TCP remoteip=LocalSubnet"; Flags: runhidden waituntilterminated
Filename: "{app}\AudioWire.exe"; Description: "Open AudioWire"; Flags: nowait postinstall skipifsilent runasoriginaluser
[UninstallRun]
Filename: "{app}\AudioWireDriverSetup.exe"; Parameters: "cleanup-startup"; Flags: runhidden waituntilterminated; RunOnceId: "RemoveAudioWireAutostart"
Filename: "{app}\AudioWireDriverSetup.exe"; Parameters: "uninstall"; Flags: runhidden waituntilterminated; RunOnceId: "RemoveAudioWireDriver"
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""AudioWire audio"""; Flags: runhidden waituntilterminated; RunOnceId: "RemoveAudioWireAudioFirewall"
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""AudioWire pairing"""; Flags: runhidden waituntilterminated; RunOnceId: "RemoveAudioWirePairingFirewall"
[Code]
procedure CurStepChanged(CurStep: TSetupStep);
#ifdef WithDriver
var ResultCode: Integer;
#endif
begin
#ifdef WithDriver
  if CurStep = ssPostInstall then begin
    if not Exec(ExpandConstant('{app}\AudioWireDriverSetup.exe'),
      'install "' + ExpandConstant('{app}\driver\audiowire.inf') + '"', '', SW_HIDE, ewWaitUntilTerminated, ResultCode)
      or ((ResultCode <> 0) and (ResultCode <> 3010)) then
      MsgBox('The AudioWire microphone driver could not be installed. Speaker mode still works. Open AudioWire for details.', mbInformation, MB_OK);
  end;
#endif
end;

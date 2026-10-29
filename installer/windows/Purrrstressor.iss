; Inno Setup script. Build: iscc /DMyVersion=1.2.3 /DSourceVst3=path\to\Purrrstressor.vst3 /DManualPdf=path\to\manual.pdf Purrrstressor.iss
#ifndef MyVersion
  #define MyVersion "0.0.0"
#endif
#ifndef SourceVst3
  #define SourceVst3 "..\..\build\Purrrstressor_artefacts\Release\VST3\Purrrstressor.vst3"
#endif
[Setup]
AppId={{6E0B1F3A-5B8C-4F27-9E55-7A1C2D3B4E87}
AppName=Purrrstressor
AppVersion={#MyVersion}
AppPublisher=Studio 87
AppPublisherURL=https://github.com/SpeakingColors/Purrrstressor
DefaultDirName={autopf}\Studio 87\Purrrstressor
DisableProgramGroupPage=yes
DisableDirPage=no
ArchitecturesInstallIn64BitMode=x64compatible
OutputBaseFilename=Purrrstressor-Setup
Compression=lzma2
SolidCompression=yes
LicenseFile=..\..\LICENSE
UninstallDisplayName=Purrrstressor
CloseApplications=yes
[Messages]
SelectDirDesc=Where should the manual and licence go?
SelectDirLabel3=The plugin itself is installed to the common VST3 folder (C:\Program Files\Common Files\VST3). This folder only holds the manual, the licence and the uninstaller.
[Files]
Source: "{#SourceVst3}\*"; DestDir: "{commoncf64}\VST3\Purrrstressor.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
#ifdef ManualPdf
Source: "{#ManualPdf}"; DestDir: "{app}"; DestName: "Purrrstressor-manual.pdf"; Flags: ignoreversion
#endif
[Code]
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  { Presets in %APPDATA%\Studio 87\Purrrstressor\presets are never touched silently. }
  if CurUninstallStep = usPostUninstall then
    if DirExists(ExpandConstant('{userappdata}\Studio 87\Purrrstressor')) then
      if MsgBox('Also delete your Purrrstressor presets?' #13#10 + ExpandConstant('{userappdata}\Studio 87\Purrrstressor'), mbConfirmation, MB_YESNO or MB_DEFBUTTON2) = IDYES then
        DelTree(ExpandConstant('{userappdata}\Studio 87\Purrrstressor'), True, True, True);
end;

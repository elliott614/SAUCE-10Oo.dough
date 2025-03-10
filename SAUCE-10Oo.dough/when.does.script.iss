[Setup]
AppName=SAUCE10OodoUgh (Sostenuto Emulator with JUCE)
AppVersion=1.0
DefaultDirName={pf}\Sostenuto Emulator with JUCE
DefaultGroupName=Sostenuto Emulator with JUCE
UninstallDisplayIcon={app}\SAUCE10oOdough.exe
Compression=lzma2
SolidCompression=yes
OutputDir=installer

[Files]
Source: "Builds\VisualStudio2019\x64\Release\App\*"; DestDir: "{app}"; Flags: recursesubdirs; Excludes: "*.tlog,*.tlog\*,*.ipch,*.ipch\*,*.obj,*.pch,*.pdb,*.ilk,*.recipe,*.recipe\*,*.idb,*.exp,*.lib,*.log,*.lastbuildstate,*.res,*.manifest,intermediates\*,vsxproj.user,*.aps"

[Icons]
Name: "{group}\Sostenuto Emulator with JUCE"; Filename: "{app}\SAUCE10oOdough.exe"
Name: "{commondesktop}\Sostenuto Emulator with JUCE"; Filename: "{app}\SAUCE10oOdough.exe"
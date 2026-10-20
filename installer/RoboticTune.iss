[Setup]
AppName=Robotic Tune
AppVersion=1.1.0
AppPublisher=ProdByPerke
AppComments=Engineered by wajfay
CreateAppDir=no
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=Output
OutputBaseFilename=RoboticTune-Setup
Compression=lzma2
SolidCompression=yes
Uninstallable=yes

[Files]
Source: "..\stage\Robotic Tune.vst3\*"; DestDir: "{commoncf64}\VST3\Robotic Tune.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion

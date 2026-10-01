#Requires -Version 7.0
param([string]$RuntimeDirectory, [string]$SdkDirectory)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $workspace 'release/v0.6.8-local/Magpie-Experimental-x64' }
if (!$SdkDirectory) { $SdkDirectory = Join-Path $workspace 'dependencies/XeSS-SDK-3.0.1' }
$output = Join-Path $workspace '.tools/xess-mfg-impl/tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /MT /O2 /DNOMINMAX /DUNICODE /D_UNICODE "/I$SdkDirectory/inc" `
    "$repo/tests/XeSSFGRuntimeSmoke.cpp" "/Fe:$output/XeSSFGRuntimeSmoke.exe" "/Fo:$output/XeSSFGRuntimeSmoke.obj"
if ($LASTEXITCODE) { throw 'XeSSFG runtime smoke compilation failed' }
& "$output/XeSSFGRuntimeSmoke.exe" $RuntimeDirectory
if ($LASTEXITCODE) { throw 'XeSSFG runtime smoke failed' }

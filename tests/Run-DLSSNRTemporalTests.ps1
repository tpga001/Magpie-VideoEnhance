#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$output = Join-Path (Split-Path $repo -Parent) '.tools/068-antiflicker/tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 "/I$repo/src/Magpie.Core" "/I$repo/src/Magpie.Core/include" "$repo/tests/DLSSNRTemporalTests.cpp" "/Fe:$output/DLSSNRTemporalTests.exe" "/Fo:$output/DLSSNRTemporalTests.obj" /link d3d11.lib d3dcompiler.lib
if ($LASTEXITCODE) { throw 'Temporal test compilation failed.' }
& "$output/DLSSNRTemporalTests.exe"
if ($LASTEXITCODE) { throw 'Temporal WARP tests failed.' }

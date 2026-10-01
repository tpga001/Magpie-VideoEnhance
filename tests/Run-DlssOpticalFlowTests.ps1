#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
$output = Join-Path $workspace '.tools/067-dlss-amdof/tests'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& python (Join-Path $PSScriptRoot 'prepare_dlss_optical_flow_test.py') $output
if ($LASTEXITCODE) { throw 'Extract production functions failed' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 "/I$output" "/I$repo/src/Magpie.Core" "/I$repo/src/Magpie.Core/include" `
    (Join-Path $PSScriptRoot 'DlssOpticalFlowTests.cpp') "/Fe:$output/DlssOpticalFlowTests.exe" "/Fo:$output/DlssOpticalFlowTests.obj"
if ($LASTEXITCODE) { throw 'Compile DLSS optical-flow tests failed' }
& "$output/DlssOpticalFlowTests.exe"
if ($LASTEXITCODE) { throw 'DLSS optical-flow tests failed' }

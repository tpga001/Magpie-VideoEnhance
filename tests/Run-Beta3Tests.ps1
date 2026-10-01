#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
$output = Join-Path $workspace '.tools/067-beta3/tests'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& python (Join-Path $PSScriptRoot 'prepare_beta3_test.py') $output
if ($LASTEXITCODE) { throw 'Beta3 fixture extraction failed' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 "/I$repo/src/Magpie" "/I$repo/src/Magpie.Core/include" "$output/beta3.cpp" "/Fe:$output/beta3.exe" "/Fo:$output/beta3.obj"
if ($LASTEXITCODE) { throw 'Beta3 test compilation failed' }
& "$output/beta3.exe"
if ($LASTEXITCODE) { throw 'Beta3 regression tests failed' }

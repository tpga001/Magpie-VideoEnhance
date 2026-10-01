#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
$output = Join-Path $workspace 'release/v0.6.7-local/obj/HdrComponentsTests'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
New-Item -ItemType Directory -Path $output -Force | Out-Null
$exe = Join-Path $output 'HdrComponentsTests.exe'
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /DNOMINMAX "/I$repo/src/Magpie.Core/include" "/I$repo/src/Magpie.Core" `
    (Join-Path $PSScriptRoot 'HdrComponentsTests.cpp') "/Fe:$exe" "/Fo:$output/HdrComponentsTests.obj"
if ($LASTEXITCODE) { throw 'HDR component plan compilation failed' }
& $exe
if ($LASTEXITCODE) { throw 'HDR component plan tests failed' }
& (Join-Path $PSScriptRoot 'Validate-EffectCatalog.ps1')

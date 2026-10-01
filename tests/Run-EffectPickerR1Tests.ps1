#Requires -Version 7.0
param([string]$CoreLibrary)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
$output = Join-Path $workspace '.tools/effect-picker-r1'
if (!$CoreLibrary) { $CoreLibrary = Join-Path $workspace 'release/v0.6.7-local/Magpie-Experimental-x64/Magpie.Core.lib' }
$CoreLibrary = (Resolve-Path -LiteralPath $CoreLibrary).Path
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
New-Item -ItemType Directory -Path $output -Force | Out-Null
function Test-Native([string]$Name, [string]$Source, [string[]]$Includes, [string[]]$Libraries, [string[]]$Arguments = @()) {
    $exe = Join-Path $output ($Name + '.exe')
    $includeArgs = @($Includes | ForEach-Object { '/I' + $_ })
    & cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT @includeArgs $Source "/Fe:$exe" "/Fo:$output/$Name.obj" /link /LTCG @Libraries
    if ($LASTEXITCODE) { throw "Compile failed: $Name" }
    & $exe @Arguments
    if ($LASTEXITCODE) { throw "Test failed: $Name" }
}
Push-Location $repo
try {
    Test-Native 'EffectPickerTests' 'tests/EffectPickerTests.cpp' @('src/Magpie.Core/include') @()
    Test-Native 'EffectParametersR1Tests' 'tests/EffectParametersR1Tests.cpp' @('src/Magpie.Core/include', 'src/Magpie.Core') @($CoreLibrary, 'WindowsApp.lib')
    Test-Native 'EffectChoiceItemsTests' 'tests/EffectChoiceItemsTests.cpp' @() @('WindowsApp.lib', '/MANIFEST:EMBED', '/MANIFESTINPUT:tests/EffectChoiceItemsTests.manifest') @('--xaml')
    & (Join-Path $PSScriptRoot 'Validate-EffectCatalog.ps1')
} finally { Pop-Location }

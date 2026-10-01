#Requires -Version 7.0
param([string]$RuntimeDirectory, [string]$IntermediateRoot)
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $workspace 'release/v0.6.7-local/Magpie-Experimental-x64' }
if (!$IntermediateRoot) { $IntermediateRoot = Join-Path $workspace 'release/v0.6.7-local/obj' }
& (Join-Path $PSScriptRoot 'Run-EffectPickerR1Tests.ps1') -CoreLibrary (Join-Path $RuntimeDirectory 'Magpie.Core.lib')
$output = Join-Path $workspace '.tools/effect-picker-r1-fix1'
New-Item -ItemType Directory -Path $output -Force | Out-Null
# Extract the actual production styles; never maintain a parallel test template.
[xml]$page = Get-Content -LiteralPath (Join-Path $repo 'src/Magpie/ScalingModesPage.xaml') -Raw
$styleNodes = @($page.Page.'Page.Resources'.ChildNodes | Where-Object {
    $_ -is [Xml.XmlElement] -and $_.GetAttribute('Key', 'http://schemas.microsoft.com/winfx/2006/xaml').StartsWith('EffectPicker')
})
if (!$styleNodes.Count) { throw 'No production picker styles found for the resource test.' }
$xaml = '<ResourceDictionary xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">' +
    ($styleNodes.OuterXml -join "`n") + '</ResourceDictionary>'
[IO.File]::WriteAllText((Join-Path $output 'EffectPickerStyles.xaml'), $xaml, [Text.UTF8Encoding]::new($false))
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$winmd = Join-Path $repo 'packages/Microsoft.UI.Xaml.2.8.7/lib/uap10.0/Microsoft.UI.Xaml.winmd'
$manifest = Join-Path $output 'WinUI.manifest'
& mt.exe -nologo "-winmd:$winmd" '-dll:Microsoft.UI.Xaml.dll' "-out:$manifest"
if ($LASTEXITCODE) { throw 'Generate the WinUI test activation manifest failed.' }
Copy-Item -LiteralPath (Join-Path $RuntimeDirectory 'Microsoft.UI.Xaml.dll') -Destination $output -Force
# Use the same trimmed WinUI PRI as the app, without Magpie's App.xbf. The
# fixture creates its own Application and never instantiates/launches Magpie.
Copy-Item -LiteralPath (Join-Path $IntermediateRoot 'x64/WinUI/Microsoft.UI.Xaml.pri') -Destination (Join-Path $output 'resources.pri') -Force
$exe = Join-Path $output 'EffectPickerThemeTests.exe'
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT "/I$IntermediateRoot/Magpie/Generated Files" `
    (Join-Path $PSScriptRoot 'EffectPickerThemeTests.cpp') "/Fe:$exe" "/Fo:$output/EffectPickerThemeTests.obj" `
    /link WindowsApp.lib /MANIFEST:EMBED "/MANIFESTINPUT:$PSScriptRoot/EffectChoiceItemsTests.manifest" "/MANIFESTINPUT:$manifest"
if ($LASTEXITCODE) { throw 'Compile picker theme tests failed.' }
Push-Location $output
try {
    & $exe
    if ($LASTEXITCODE) { throw 'Picker theme tests failed.' }
} finally { Pop-Location }

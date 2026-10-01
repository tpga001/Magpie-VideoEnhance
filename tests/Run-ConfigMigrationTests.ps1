#Requires -Version 7.0
$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
$output = Join-Path $workspace '.tools/067-v4e-config/tests'
New-Item -ItemType Directory -Path $output -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$rapid = Get-ChildItem -LiteralPath (Join-Path $env:USERPROFILE '.conan2/p') -Directory -Filter 'rapid*' |
    Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'p/include/rapidjson/document.h') } | Select-Object -First 1
if (!$rapid) { throw 'Restore rapidjson before running configuration tests.' }
$helperSource = Get-Content -LiteralPath (Join-Path $repo 'src/Magpie.Core/Win32Helper.cpp') -Raw
$helperStart = $helperSource.IndexOf('bool Win32Helper::DirExists(')
$helperEnd = $helperSource.IndexOf('const Win32Helper::OSVersion&', $helperStart)
if ($helperStart -lt 0 -or $helperEnd -le $helperStart) { throw 'Configuration directory fixture extraction failed.' }
$helperSource.Substring($helperStart, $helperEnd - $helperStart) |
    Set-Content -LiteralPath (Join-Path $output 'ConfigDirectoryHelpers.inc') -Encoding utf8
foreach ($test in @('tests/ConfigLocationsTests.cpp', 'tests/ConfigDirectoryTests.cpp', 'scripts/tests/config_recovery.cpp')) {
    $name = [IO.Path]::GetFileNameWithoutExtension($test)
    & cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /DNOMINMAX /DUNICODE /D_UNICODE "/I$output" "/I$repo/src/Magpie" "/I$($rapid.FullName)/p/include" `
        "$repo/$test" "/Fe:$output/$name.exe" "/Fo:$output/$name.obj"
    if ($LASTEXITCODE) { throw "Configuration test compilation failed: $name" }
    & "$output/$name.exe" $output
    if ($LASTEXITCODE) { throw "Configuration test failed: $name" }
}
& python "$repo/scripts/tests/test_scaling_prerequisites.py" $output
if ($LASTEXITCODE) { throw 'Extract startup prerequisite tests failed' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 "$output/scaling_prerequisites.cpp" `
    "/Fe:$output/scaling_prerequisites.exe" "/Fo:$output/scaling_prerequisites.obj"
if ($LASTEXITCODE) { throw 'Startup prerequisite test compilation failed' }
& "$output/scaling_prerequisites.exe"
if ($LASTEXITCODE) { throw 'Startup prerequisite tests failed' }

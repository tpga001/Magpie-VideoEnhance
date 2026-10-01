#Requires -Version 7.0
param([string]$RapidJsonIncludeDirectory)
$ErrorActionPreference = 'Stop'
$syncRepo = Split-Path $PSScriptRoot -Parent
$syncWorkspace = Split-Path $syncRepo -Parent
$syncOutput = Join-Path $syncWorkspace 'release/v0.6.7-local/obj/FrameSyncModesTests'
New-Item -ItemType Directory -Path $syncOutput -Force | Out-Null
$syncVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$syncVs = & $syncVswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $syncVs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $syncVs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX "/I$syncRepo/src/Magpie.Core/include" `
    (Join-Path $PSScriptRoot 'FrameSyncModesTests.cpp') "/Fe:$syncOutput/policy.exe" "/Fo:$syncOutput/policy.obj"
if ($LASTEXITCODE) { throw 'Frame sync policy compilation failed' }
& "$syncOutput/policy.exe"
if ($LASTEXITCODE) { throw 'Frame sync policy tests failed' }
& python (Join-Path $PSScriptRoot 'prepare_frame_sync_timer_test.py') $syncOutput
if ($LASTEXITCODE) { throw 'Extract production StepTimer failed' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX "/I$syncRepo/src/Magpie.Core" `
    "$syncOutput/frame_sync_timer.cpp" "/Fe:$syncOutput/timer.exe" "/Fo:$syncOutput/timer.obj"
if ($LASTEXITCODE) { throw 'Production StepTimer test compilation failed' }
& "$syncOutput/timer.exe"
if ($LASTEXITCODE) { throw 'Production StepTimer tests failed' }
& python (Join-Path $PSScriptRoot 'prepare_frame_sync_runtime_test.py') $syncOutput
if ($LASTEXITCODE) { throw 'Extract production limiter configuration failed' }
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX "/I$syncRepo/src/Magpie.Core" "/I$syncRepo/src/Magpie.Core/include" `
    "$syncOutput/frame_sync_runtime.cpp" "/Fe:$syncOutput/runtime.exe" "/Fo:$syncOutput/runtime.obj"
if ($LASTEXITCODE) { throw 'Production limiter configuration test compilation failed' }
& "$syncOutput/runtime.exe"
if ($LASTEXITCODE) { throw 'Production limiter configuration tests failed' }
foreach ($negative in @('negative_policy/runtime', 'negative_double')) {
    & cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /W4 /WX "/I$syncRepo/src/Magpie.Core" "/I$syncRepo/src/Magpie.Core/include" `
        "$syncOutput/$negative.cpp" "/Fe:$syncOutput/$negative.exe" "/Fo:$syncOutput/$negative.obj"
    if ($LASTEXITCODE) { throw "Negative regression failed to compile: $negative" }
    & "$syncOutput/$negative.exe"
    if ($LASTEXITCODE -ne 42) { throw "Negative regression was not detected: $negative" }
    Write-Host "PASS: rejected production mutation $negative"
}
if (!$RapidJsonIncludeDirectory) {
    $syncRapidJson = Get-ChildItem -LiteralPath (Join-Path $env:USERPROFILE '.conan2/p') -Directory -Filter 'rapid*' |
        Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'p/include/rapidjson/document.h') } | Select-Object -First 1
    if (!$syncRapidJson) { throw 'Set RapidJsonIncludeDirectory to the restored rapidjson include directory.' }
    $RapidJsonIncludeDirectory = Join-Path $syncRapidJson.FullName 'p/include'
}
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /MT /O2 /DNOMINMAX "/I$syncRepo/src/Magpie" "/I$RapidJsonIncludeDirectory" `
    "$syncRepo/scripts/tests/config_recovery.cpp" "/Fe:$syncOutput/recovery.exe" "/Fo:$syncOutput/recovery.obj"
if ($LASTEXITCODE) { throw 'Configuration recovery test compilation failed' }
& "$syncOutput/recovery.exe" $syncOutput
if ($LASTEXITCODE) { throw 'Configuration recovery tests failed' }

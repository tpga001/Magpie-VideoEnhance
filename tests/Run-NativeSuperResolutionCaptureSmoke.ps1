#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [ValidateRange(6, 60)][int]$Seconds = 8,
    [ValidateSet(10, 11, 12, 13)][int]$DlssModelPreset = 11,
    [switch]$DlssOnly
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
if (Get-Process Magpie -ErrorAction SilentlyContinue) {
    throw 'Close the installed Magpie before the isolated native-SR smoke test.'
}
if (Get-Process MagpieNativeSrCaptureApp -ErrorAction SilentlyContinue) {
    throw 'A native-SR smoke test is already running.'
}

$manifestPath = Join-Path $RuntimeDirectory 'build-manifest.json'
if (!(Test-Path -LiteralPath $manifestPath)) { throw 'The candidate runtime has no build manifest.' }
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
foreach ($option in @('EnableDLSSSR', 'EnableXeSSZeroMV', 'EnableFSR3ZeroMV')) {
    if ($manifest.featureOptions.$option -ne 'true') { throw "Candidate runtime does not contain $option" }
}

$output = Join-Path $workspace ('.tools/native-super-resolution/capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$runtime = Join-Path $output 'runtime'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
foreach ($file in $manifest.files) {
    if ([IO.Path]::GetExtension($file.path) -in @('.pdb', '.map')) { continue }
    $target = Join-Path $runtime $file.path
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $RuntimeDirectory $file.path) -Destination $target
}

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath | Select-Object -First 1
Import-Module (Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$fixture = Join-Path $output 'SpatialUpscalerCaptureFixture.exe'
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /MT /O2 `
    "$repo/tests/SpatialUpscalerCaptureFixture.cpp" "/Fe:$fixture" "/Fo:$output/fixture.obj"
if ($LASTEXITCODE) { throw 'Capture fixture compilation failed' }

$dlssPresetName = @{ 10 = 'J'; 11 = 'K'; 12 = 'L'; 13 = 'M' }[$DlssModelPreset]
$cases = @(
    [ordered]@{
        mode = 1
        name = 'DLSS SR'
        effect = 'DLSS\DLSS_SR'
        success = "DLSS SR initialized \(Performance, preset $dlssPresetName\): 938x524 -> 1876x1048"
        required = 'DLSS SR guidance frameId=1: requested motion=false, bound motion=zero depth=zero, fallback=none'
        failures = @('DLSS SR is disabled at build time', 'NVSDK_NGX_D3D11_Init failed',
            'NVSDK_NGX_D3D11_GetCapabilityParameters failed', 'NGX_D3D11_CREATE_DLSS_EXT failed',
            'NGX_D3D11_EVALUATE_DLSS_EXT failed')
    },
    [ordered]@{
        mode = 2
        name = 'XeSS SR'
        effect = 'XeSS\XeSS_SR'
        success = 'XeSS experimental D3D11/D3D12 backend initialized .*: 938x524 -> 1876x1048'
        required = $null
        failures = @('XeSS SR support is not enabled in this build', 'xessD3D12CreateContext failed',
            'xessD3D12Init failed', 'xessD3D12Execute failed')
    },
    [ordered]@{
        mode = 3
        name = 'FSR3 SR'
        effect = 'FSR3\FSR3_SR'
        success = 'FSR 3\.1\.5 D3D11/D3D12 backend initialized \(opticalFlow=false, virtual auxiliary inputs\): 938x524 -> 1876x1048'
        required = $null
        failures = @('FSR3 support is not enabled in this build', 'Load amd_fidelityfx_loader_dx12.dll failed',
            'FSR 3.1.5 provider not found', 'Create FSR 3.1.5 context failed', 'Dispatch FSR 3.1.5 failed')
    }
)
if ($DlssOnly) { $cases = @($cases[0]) }

$scalingModes = @()
$profiles = @(@{ scalingMode = -1 })
foreach ($case in $cases) {
    $parameters = @{
        dlssModelPreset = $DlssModelPreset
        opticalFlowMethod = 0
        amdOpticalFlowMode = 1
        nvidiaOpticalFlowQuality = 2
    }
    $scalingModes += @{
        name = $case.name
        effects = @(@{ name = $case.effect; scalingType = 1; scale = @{ x = 1; y = 1 }; parameters = $parameters })
    }
    $profiles += @{
        name = $case.name
        packaged = $false
        pathRule = $fixture
        classNameRule = "MagpieSpatialCapture$($case.mode)"
        autoScale = 2
        scalingMode = $scalingModes.Count - 1
        captureMethod = 0
        initialWindowedScaleFactor = 4
        parameterFocusSwitching = $false
        enableHdrCompatibility = $false
    }
}
$config = @{
    alwaysRunAsAdmin = $false
    autoCheckForUpdates = $false
    showNotifyIcon = $true
    simulateExclusiveFullscreen = $false
    frontEdgeSync = $true
    frontEdgeSyncFrameRate = 60
    frameSyncMode = 1
    vrr = $false
    stopEffectsOnTaskSwitch = $false
    experimentalDlssSrSettingsVersion = 1
    experimentalOpticalFlowDefaultsVersion = 1
    profiles = $profiles
    scalingModes = $scalingModes
}
$configDir = Join-Path $runtime 'config/v4e'
New-Item -ItemType Directory -Path $configDir -Force | Out-Null
$config | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $configDir 'config.json') -Encoding utf8

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class NativeSrSmokeQuit {
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wp, IntPtr lp);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
}
'@

$appPath = Join-Path $runtime 'MagpieNativeSrCaptureApp.exe'
Copy-Item -LiteralPath (Join-Path $runtime 'Magpie.exe') -Destination $appPath
$app = Start-Process -FilePath $appPath -ArgumentList '-t' -WorkingDirectory $runtime -WindowStyle Hidden -PassThru
Write-Output "Isolated native-SR runtime: $runtime"

function Read-MagpieLog {
    $files = @(Get-ChildItem -LiteralPath (Join-Path $runtime 'logs') -Filter 'magpie*.log' -ErrorAction SilentlyContinue |
        Sort-Object Name)
    return ($files | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
}

$observations = @()
try {
    Start-Sleep -Seconds 3
    $previousSeconds = $env:MAGPIE_CAPTURE_SECONDS
    try {
        $env:MAGPIE_CAPTURE_SECONDS = "$Seconds"
        foreach ($case in $cases) {
            $before = Read-MagpieLog
            & $fixture $case.mode
            $fixtureExitCode = $LASTEXITCODE
            Start-Sleep -Milliseconds 500
            $after = Read-MagpieLog
            $segment = if ($after.Length -ge $before.Length) { $after.Substring($before.Length) } else { $after }
            $segmentPath = Join-Path $output ("$($case.mode)-" + ($case.name -replace '[^A-Za-z0-9]+', '-') + '.log')
            Set-Content -LiteralPath $segmentPath -Value $segment -Encoding utf8
            $failureMatches = @($case.failures | Where-Object { $segment -match [regex]::Escape($_) })
            $genericFailure = $segment -match ('Initialize native effect ' + [regex]::Escape($case.effect) + ' failed|Native effect Draw failed: effect=' + [regex]::Escape($case.effect))
            $observation = [ordered]@{
                mode = $case.mode
                name = $case.name
                effect = $case.effect
                fixtureExitCode = $fixtureExitCode
                initialized = $segment -match $case.success
                requiredDetail = !$case.required -or $segment -match [regex]::Escape($case.required)
                target2x = $segment -match '目标矩形: [^\r\n]*\(1876x1048\)'
                firstFrontendFrame = $segment -match 'First frontend frame submitted by regular renderer'
                sustained120 = $segment -match 'Backend publication CPU timing: samples=120'
                failureMatches = $failureMatches
                genericFailure = $genericFailure
                log = $segmentPath
            }
            $observation.passed = $fixtureExitCode -eq 0 -and $observation.initialized -and
                $observation.requiredDetail -and $observation.target2x -and
                $observation.firstFrontendFrame -and $observation.sustained120 -and
                !$observation.genericFailure -and $failureMatches.Count -eq 0
            $observations += $observation
        }
    } finally {
        $env:MAGPIE_CAPTURE_SECONDS = $previousSeconds
    }
} finally {
    $window = [NativeSrSmokeQuit]::FindWindow('Magpie_NotifyIcon', $null)
    [uint32]$owner = 0
    [void][NativeSrSmokeQuit]::GetWindowThreadProcessId($window, [ref]$owner)
    if ($owner -eq $app.Id) {
        [void][NativeSrSmokeQuit]::PostMessage(
            $window,
            [NativeSrSmokeQuit]::RegisterWindowMessage('WM_MAGPIE_QUIT'),
            [IntPtr]::Zero,
            [IntPtr]::Zero)
    }
    if (!$app.WaitForExit(15000)) {
        Stop-Process -Id $app.Id -Force
        throw "Test Magpie did not exit cleanly: PID $($app.Id)"
    }
}

$result = [ordered]@{
    passed = $observations.Count -eq $cases.Count -and !($observations.passed -contains $false)
    runtime = $runtime
    candidateRuntime = $RuntimeDirectory
    commit = $manifest.commit
    secondsPerMode = $Seconds
    source = @{ width = 938; height = 524 }
    output = @{ width = 1876; height = 1048 }
    dlssModelPreset = $DlssModelPreset
    observations = $observations
}
$resultPath = Join-Path $output 'capture-verification.json'
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resultPath -Encoding utf8
Write-Output "Native-SR result: $resultPath"
if (!$result.passed) { throw 'One or more native super-resolution smoke tests failed.' }
if ($DlssOnly) {
    Write-Output "DLSS SR preset $dlssPresetName actual WGC 2x capture passed."
} else {
    Write-Output 'DLSS SR, XeSS SR and FSR3 SR actual WGC 2x capture passed.'
}

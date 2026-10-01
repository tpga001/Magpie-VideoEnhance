#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [ValidateRange(2, 60)][int]$Seconds = 6
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
if (Get-Process Magpie -ErrorAction SilentlyContinue) {
    throw 'Close Magpie before the isolated spatial-upscaler smoke test.'
}

$output = Join-Path $workspace ('.tools/spatial-upscaler/capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$runtime = Join-Path $output 'runtime'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null

# Copy only the files recorded by the package manifest. User settings are never copied.
$manifest = Get-Content -LiteralPath (Join-Path $RuntimeDirectory 'build-manifest.json') -Raw | ConvertFrom-Json
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

$scalingModes = @(
    @{
        name = 'FSR1 2x smoke'
        effects = @(
            @{ name = 'FSR\FSR_EASU'; scalingType = 1; scale = @{ x = 1; y = 1 } },
            @{ name = 'FSR\FSR_RCAS'; scalingType = 0; scale = @{ x = 1; y = 1 }; parameters = @{ sharpness = 0.87 } }
        )
    },
    @{
        name = 'NIS 2x smoke'
        effects = @(
            @{ name = 'NIS\NIS'; scalingType = 1; scale = @{ x = 1; y = 1 }; parameters = @{ sharpness = 0.5 } }
        )
    }
)
$profiles = @(@{ scalingMode = -1 })
for ($index = 0; $index -lt $scalingModes.Count; ++$index) {
    $mode = $index + 1
    $profiles += @{
        name = $scalingModes[$index].name
        packaged = $false
        pathRule = $fixture
        classNameRule = "MagpieSpatialCapture$mode"
        autoScale = 2
        scalingMode = $index
        captureMethod = 0
        # Persisted enum value 4 means a 2x windowed output.
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
    profiles = $profiles
    scalingModes = $scalingModes
}
$configDir = Join-Path $runtime 'config/v4e'
New-Item -ItemType Directory -Path $configDir -Force | Out-Null
$config | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $configDir 'config.json') -Encoding utf8

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class SpatialSmokeQuit {
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wp, IntPtr lp);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
}
'@

$app = Start-Process -FilePath (Join-Path $runtime 'Magpie.exe') -ArgumentList '-t' `
    -WorkingDirectory $runtime -WindowStyle Hidden -PassThru
Write-Output "Isolated spatial-upscaler runtime: $runtime"
try {
    Start-Sleep -Seconds 3
    $previousSeconds = $env:MAGPIE_CAPTURE_SECONDS
    try {
        $env:MAGPIE_CAPTURE_SECONDS = "$Seconds"
        & $fixture 1 2
    } finally {
        $env:MAGPIE_CAPTURE_SECONDS = $previousSeconds
    }
    if ($LASTEXITCODE) { throw 'Capture fixture failed; inspect isolated runtime logs.' }
} finally {
    $window = [SpatialSmokeQuit]::FindWindow('Magpie_NotifyIcon', $null)
    [uint32]$owner = 0
    [void][SpatialSmokeQuit]::GetWindowThreadProcessId($window, [ref]$owner)
    if ($owner -eq $app.Id) {
        [void][SpatialSmokeQuit]::PostMessage(
            $window,
            [SpatialSmokeQuit]::RegisterWindowMessage('WM_MAGPIE_QUIT'),
            [IntPtr]::Zero,
            [IntPtr]::Zero)
    }
    if (!$app.WaitForExit(15000)) { throw "Test Magpie did not exit: PID $($app.Id)" }
}
if ($app.ExitCode) { throw "Test Magpie exited with code $($app.ExitCode)" }

$logFiles = @(Get-ChildItem -LiteralPath (Join-Path $runtime 'logs') -Filter 'magpie*.log' |
    Sort-Object @{ Expression = { if ($_.Name -match '^magpie\.(\d+)\.log$') { [int]$Matches[1] } else { 0 } }; Descending = $true })
$log = ($logFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
$checks = [ordered]@{
    fsrEasuCompiled = $log -match 'FSR\\FSR_EASU\.hlsl'
    fsrRcasCompiled = $log -match 'FSR\\FSR_RCAS\.hlsl'
    nisCompiled = $log -match 'NIS\\NIS\.hlsl'
    firstFrontendFrames = [regex]::Matches($log, 'First frontend frame submitted').Count
    twoXTargets = [regex]::Matches($log, '目标矩形: [^\r\n]*\(1876x1048\)').Count
}
if (!$checks.fsrEasuCompiled -or !$checks.fsrRcasCompiled -or !$checks.nisCompiled) {
    throw 'One or more spatial shaders did not compile or load.'
}
if ($checks.firstFrontendFrames -lt 2) { throw 'Both spatial-upscaler sessions did not present a frontend frame.' }
if ($checks.twoXTargets -lt 2) { throw 'Both sessions did not produce the expected 2x target geometry.' }
if ($log -match '编译着色器失败|Compile effect failed|初始化效果失败|Initialize effect failed') {
    throw 'Runtime reported an effect initialization or compilation failure.'
}

$result = [ordered]@{
    passed = $true
    runtime = $runtime
    secondsPerMode = $Seconds
    source = @{ width = 938; height = 524 }
    output = @{ width = 1876; height = 1048 }
    effects = @(
        @{ name = 'FSR1 EASU + RCAS'; compiled = $checks.fsrEasuCompiled -and $checks.fsrRcasCompiled; presented = $true },
        @{ name = 'NIS'; compiled = $checks.nisCompiled; presented = $true }
    )
    firstFrontendFrames = $checks.firstFrontendFrames
}
$resultPath = Join-Path $output 'capture-verification.json'
$result | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $resultPath -Encoding utf8
Write-Output "FSR1 and NIS actual WGC 2x capture passed. Result: $resultPath"

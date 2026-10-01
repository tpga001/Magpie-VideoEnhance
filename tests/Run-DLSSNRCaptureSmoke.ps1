#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [Parameter(Mandatory)][string]$DlssnrRuntimePath,
    [ValidateRange(3, 120)][int]$Seconds = 12,
    [switch]$Fullscreen
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent

if (Get-Process Magpie -ErrorAction SilentlyContinue) {
    throw 'Close Magpie before the isolated DLSSNR capture smoke test.'
}
if (Get-Process MagpieDLSSNRCaptureApp -ErrorAction SilentlyContinue) {
    throw 'A DLSSNR capture smoke test is already running.'
}

$candidate = (Resolve-Path -LiteralPath $DlssnrRuntimePath).Path
if ([IO.Path]::GetFileName($candidate) -ne 'nvngx_dlssnr.dll') {
    throw 'The candidate must be named nvngx_dlssnr.dll.'
}
$manifestPath = Join-Path $RuntimeDirectory 'build-manifest.json'
if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Missing package manifest: $manifestPath"
}

$output = Join-Path $workspace (
    '.tools/dlssnr-smoke/capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$runtime = Join-Path $output 'runtime'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null

# Copy only release-manifest files. User configs, logs and caches are not read.
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
foreach ($file in $manifest.files) {
    if ([IO.Path]::GetExtension($file.path) -in @('.pdb', '.map')) { continue }
    $source = Join-Path $RuntimeDirectory $file.path
    $target = Join-Path $runtime $file.path
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $target
}

$isolatedCandidate = Join-Path $runtime 'nvngx_dlssnr.dll'
Copy-Item -LiteralPath $candidate -Destination $isolatedCandidate -Force
$candidateInfo = Get-Item -LiteralPath $isolatedCandidate
$candidateHash = (Get-FileHash -LiteralPath $isolatedCandidate -Algorithm SHA256).Hash
$candidateSignature = Get-AuthenticodeSignature -LiteralPath $isolatedCandidate

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -property installationPath | Select-Object -First 1
if (!$vs) { throw 'A Visual C++ toolchain is required to build the capture fixture.' }
$fixture = Join-Path $output 'MagpieDLSSNRCaptureFixture.exe'
$fixtureSource = Join-Path $repo 'tests/XeSSFGCaptureFixture.cpp'
$devShell = Join-Path $vs 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll'
if (Test-Path -LiteralPath $devShell) {
    Import-Module $devShell
    Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation `
        -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
    & cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /WX /MT /O2 `
        $fixtureSource "/Fe:$fixture" "/Fo:$output/fixture.obj"
} else {
    # VS 2017 does not ship Microsoft.VisualStudio.DevShell.dll.
    $devCmd = Join-Path $vs 'Common7/Tools/VsDevCmd.bat'
    if (!(Test-Path -LiteralPath $devCmd -PathType Leaf)) {
        throw "Visual Studio developer environment not found: $devCmd"
    }
    $compile = 'call "{0}" -arch=x64 -host_arch=x64 >nul && cl.exe /nologo /std:c++17 /EHsc /utf-8 /W4 /WX /MT /O2 "{1}" /Fe:"{2}" /Fo:"{3}"' -f `
        $devCmd, $fixtureSource, $fixture, (Join-Path $output 'fixture.obj')
    & cmd.exe /d /s /c $compile
}
if ($LASTEXITCODE) { throw 'Capture fixture compilation failed.' }

$effects = @(@{
    name = 'DLSSNR\DLSSNR_AI_Filter'
    scalingType = 0
    scale = @{x = 1; y = 1}
    parameters = @{
        opticalFlowMethod = 0
        enableInputResolutionScaling = 0
        inputResolutionPercent = 100
        multiPass = 1
        antiFlicker = 0
    }
})
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
    smoothMotionCompatibilityMode = $false
    experimentalDlssnrSettingsVersion = 2
    profiles = @(
        @{scalingMode = -1},
        @{
            name = 'DLSSNR isolated capture'
            packaged = $false
            pathRule = $fixture
            classNameRule = 'MagpieXeSSCapture5'
            autoScale = $(if ($Fullscreen) { 1 } else { 2 })
            scalingMode = 0
            captureMethod = 0
            initialWindowedScaleFactor = 7
            parameterFocusSwitching = $false
            enableHdrCompatibility = $false
        }
    )
    scalingModes = @(@{name = 'DLSSNR isolated capture'; effects = $effects})
}
$configDir = Join-Path $runtime 'config/v4e'
New-Item -ItemType Directory -Path $configDir -Force | Out-Null
$config | ConvertTo-Json -Depth 12 |
    Set-Content -LiteralPath (Join-Path $configDir 'config.json') -Encoding utf8

Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class DLSSNRCaptureQuit {
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wp, IntPtr lp);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
}
'@

$appPath = Join-Path $runtime 'MagpieDLSSNRCaptureApp.exe'
Copy-Item -LiteralPath (Join-Path $runtime 'Magpie.exe') -Destination $appPath
$app = Start-Process -FilePath $appPath -ArgumentList '-t' -WorkingDirectory $runtime `
    -WindowStyle Hidden -PassThru
Write-Output "Isolated DLSSNR runtime: $runtime"
try {
    Start-Sleep -Seconds 3
    $previousSeconds = $env:MAGPIE_CAPTURE_SECONDS
    try {
        $env:MAGPIE_CAPTURE_SECONDS = "$Seconds"
        & $fixture 5
    } finally {
        $env:MAGPIE_CAPTURE_SECONDS = $previousSeconds
    }
    if ($LASTEXITCODE) { throw 'Capture fixture failed; inspect the isolated runtime logs.' }
} finally {
    $window = [DLSSNRCaptureQuit]::FindWindow('Magpie_NotifyIcon', $null)
    [uint32]$owner = 0
    [void][DLSSNRCaptureQuit]::GetWindowThreadProcessId($window, [ref]$owner)
    if ($owner -eq $app.Id) {
        [void][DLSSNRCaptureQuit]::PostMessage(
            $window,
            [DLSSNRCaptureQuit]::RegisterWindowMessage('WM_MAGPIE_QUIT'),
            [IntPtr]::Zero,
            [IntPtr]::Zero)
    }
    if (!$app.WaitForExit(15000)) {
        throw "Test Magpie did not exit: PID $($app.Id), runtime $runtime"
    }
}
if ($app.ExitCode) { throw "Test Magpie exited with code $($app.ExitCode)" }

$logFiles = @(Get-ChildItem -LiteralPath (Join-Path $runtime 'logs') -Filter 'magpie*.log' |
    Sort-Object LastWriteTime)
if (!$logFiles.Count) { throw 'The isolated Magpie process produced no log.' }
$log = ($logFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
if ($log -notmatch 'DLSSNR STATUS: Feature=18 .*created=true') {
    throw 'DLSSNR Feature 18 did not initialize in the isolated Magpie runtime.'
}
if ($log -match 'Feature 18 creation failed|fallback=pass-through|DLSSNR EvaluateFeature failed') {
    throw 'The isolated Magpie runtime reported a DLSSNR create/evaluate failure.'
}

$verification = @{
    runtime = $runtime
    sourceRuntime = (Resolve-Path -LiteralPath $RuntimeDirectory).Path
    candidate = @{
        path = $candidate
        sha256 = $candidateHash
        bytes = $candidateInfo.Length
        fileVersion = $candidateInfo.VersionInfo.FileVersion
        signature = $candidateSignature.Status.ToString()
        signerThumbprint = if ($candidateSignature.SignerCertificate) {
            $candidateSignature.SignerCertificate.Thumbprint
        } else { $null }
    }
    seconds = $Seconds
    fullscreen = [bool]$Fullscreen
    feature18Created = $true
    logFiles = @($logFiles.FullName)
    executableSha256 = (Get-FileHash -LiteralPath $appPath -Algorithm SHA256).Hash
}
$verification | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $output 'capture-verification.json') -Encoding utf8
Write-Output 'Actual WGC DLSSNR capture passed in an isolated portable runtime.'

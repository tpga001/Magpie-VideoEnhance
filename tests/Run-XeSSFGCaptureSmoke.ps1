#Requires -Version 7.0
param([string]$RuntimeDirectory, [switch]$IsolateProcessName, [int[]]$Modes = @(2,3,4,2,5),
    [ValidateRange(1,900)][int]$Seconds = 12, [switch]$StartupTrace, [switch]$Fullscreen,
    [ValidateSet('AMD','NVIDIA','None')][string]$OpticalFlow = 'AMD',
    [ValidateRange(1,5)][int]$NvidiaQuality = 2)
$ErrorActionPreference = 'Stop'
if ($StartupTrace -and $Modes.Count -gt 2) {
    throw 'Startup tracing retains at most two sessions per run within the rotating log budget; split longer sequences or omit -StartupTrace.'
}
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
if (!$RuntimeDirectory) { $RuntimeDirectory = Join-Path $workspace 'release/v0.6.8-local/Magpie-Experimental-x64' }
if (Get-Process Magpie -ErrorAction SilentlyContinue) { throw 'Close Magpie before the isolated capture smoke test.' }
if (Get-Process MagpieXeSSCaptureApp -ErrorAction SilentlyContinue) { throw 'A capture test is already running.' }
$output = Join-Path $workspace ('.tools/xess-mfg-impl/capture-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$runtime = Join-Path $output 'runtime'
New-Item -ItemType Directory -Path $runtime | Out-Null
# Copy only known package artifacts, never user configs, caches or logs.
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
$fixture = Join-Path $output 'XeSSFGCaptureFixture.exe'
& cl.exe /nologo /std:c++20 /EHsc /utf-8 /W4 /WX /MT /O2 "$repo/tests/XeSSFGCaptureFixture.cpp" "/Fe:$fixture" "/Fo:$output/fixture.obj"
if ($LASTEXITCODE) { throw 'Capture fixture compilation failed' }
$scalingModes = @()
$profiles = @(@{scalingMode=-1})
$flowMethod = @{None=0; AMD=1; NVIDIA=2}[$OpticalFlow]
foreach ($mode in @(2,3,4,5)) {
    $multiplier = if ($mode -eq 5) { 4 } else { $mode }
    $effects = @(@{name='FrameRate_Filter'; scalingType=0; scale=@{x=1;y=1}})
    if ($mode -eq 5) {
        $effects += @{name='DLSSNR\DLSSNR_AI_Filter'; scalingType=0; scale=@{x=1;y=1}; parameters=@{
            opticalFlowMethod=1; amdOpticalFlowMode=1; enableInputResolutionScaling=1; inputResolutionPercent=50; multiPass=1; antiFlicker=4
        }}
    }
    $effects += @{name='XeSSFG\XeSS_FrameGeneration'; scalingType=0; scale=@{x=1;y=1}; parameters=@{
        multiplier=$multiplier; opticalFlowMethod=$flowMethod; amdOpticalFlowMode=1; nvidiaOpticalFlowQuality=$NvidiaQuality
    }}
    $profiles += @{
        name="Capture $mode"; packaged=$false; pathRule=$fixture; classNameRule="MagpieXeSSCapture$mode";
        autoScale=$(if ($Fullscreen) { 1 } else { 2 }); scalingMode=$scalingModes.Count; captureMethod=0; initialWindowedScaleFactor=7;
        parameterFocusSwitching=$false; enableHdrCompatibility=$false
    }
    $scalingModes += @{name="Capture $mode"; effects=$effects}
}
$config = @{
    alwaysRunAsAdmin=$false; autoCheckForUpdates=$false; showNotifyIcon=$true; simulateExclusiveFullscreen=$false;
    frontEdgeSync=$true; frontEdgeSyncFrameRate=60; frameSyncMode=1; vrr=$false; stopEffectsOnTaskSwitch=$false;
    smoothMotionCompatibilityMode=$false; experimentalXeSSFGSettingsVersion=1; experimentalDlssnrSettingsVersion=2;
    experimentalDlssSrSettingsVersion=1; experimentalDepthRemovalVersion=1; experimentalOpticalFlowDefaultsVersion=1;
    profiles=$profiles; scalingModes=$scalingModes
}
$configDir = Join-Path $runtime 'config/v4e'
New-Item -ItemType Directory -Path $configDir -Force | Out-Null
$config | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $configDir 'config.json') -Encoding utf8
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class XeSSCaptureQuit {
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
 [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
 [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wp, IntPtr lp);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
}
'@
$appPath = Join-Path $runtime 'Magpie.exe'
if ($IsolateProcessName) {
    # Optional diagnostic control for third-party injection profiles keyed by EXE name.
    $appPath = Join-Path $runtime 'MagpieXeSSCaptureApp.exe'
    Copy-Item -LiteralPath (Join-Path $runtime 'Magpie.exe') -Destination $appPath
}
$launchEnvironment = @{}
if ($StartupTrace) { $launchEnvironment['MAGPIE_XESS_STARTUP_TRACE'] = '1' }
$app = Start-Process -FilePath $appPath -ArgumentList '-t' -WorkingDirectory $runtime -WindowStyle Hidden -PassThru -Environment $launchEnvironment
Write-Output "Isolated capture runtime: $runtime"
try {
    Start-Sleep -Seconds 3
    $previousSeconds = $env:MAGPIE_CAPTURE_SECONDS
    try {
        $env:MAGPIE_CAPTURE_SECONDS = "$Seconds"
        & $fixture @Modes
    } finally { $env:MAGPIE_CAPTURE_SECONDS = $previousSeconds }
    if ($LASTEXITCODE) { throw 'Capture fixture failed; inspect isolated runtime logs.' }
    $rtssInjected = [bool](Get-Process -Id $app.Id -Module | Where-Object ModuleName -eq 'RTSSHooks64.dll')
} finally {
    $window = [XeSSCaptureQuit]::FindWindow('Magpie_NotifyIcon', $null)
    [uint32]$owner = 0
    [void][XeSSCaptureQuit]::GetWindowThreadProcessId($window, [ref]$owner)
    if ($owner -eq $app.Id) {
        [void][XeSSCaptureQuit]::PostMessage($window, [XeSSCaptureQuit]::RegisterWindowMessage('WM_MAGPIE_QUIT'), [IntPtr]::Zero, [IntPtr]::Zero)
    }
    if (!$app.WaitForExit(15000)) { throw "Test Magpie did not exit: PID $($app.Id), runtime $runtime" }
}
if ($app.ExitCode) { throw "Test Magpie exited with code $($app.ExitCode)" }
$logFiles = @(Get-ChildItem -LiteralPath (Join-Path $runtime 'logs') -Filter 'magpie*.log' |
    Sort-Object @{Expression={ if ($_.Name -match '^magpie\.(\d+)\.log$') { [int]$Matches[1] } else { 0 } }; Descending=$true})
$log = ($logFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
$sessions = [regex]::Matches($log, 'XeSSFG request: [^\r\n]*requestedMultiplier=(\d)x')
if ($sessions.Count -ne $Modes.Count) { throw 'Expected capture sessions were not all initialized' }
$observations = @()
for ($i = 0; $i -lt $sessions.Count; ++$i) {
    $multiplier = if ($Modes[$i] -eq 5) { 4 } else { $Modes[$i] }
    if ([int]$sessions[$i].Groups[1].Value -ne $multiplier) { throw 'Unexpected capture session multiplier' }
    $end = if ($i + 1 -lt $sessions.Count) { $sessions[$i + 1].Index } else { $log.Length }
    $sessionLog = $log.Substring($sessions[$i].Index, $end - $sessions[$i].Index)
    if ($sessionLog -notmatch "opticalFlowMethod=$flowMethod,") { throw 'Wrong optical-flow method initialized' }
    if ($OpticalFlow -eq 'NVIDIA') {
        $profile = @('4F','4M','4S','2M','2S')[$NvidiaQuality - 1]
        if ($sessionLog -notmatch "Frame Guidance NVOF initialized: profile=$profile ") { throw 'Requested NVOF quality did not initialize' }
    }
    $sdkMatches = [regex]::Matches($sessionLog, "XeSSFG SDK output: requested=${multiplier}x frames=(\d+) submissions=(\d+) partialBursts=(\d+)")
    if (!$sdkMatches.Count) { throw "No ${multiplier}x SDK observations" }
    foreach ($match in $sdkMatches) {
        $frames = [double]$match.Groups[1].Value; $submissions = [double]$match.Groups[2].Value
        if ($StartupTrace -and $submissions -lt 120) { continue }
        if ($frames/$submissions -lt $multiplier*0.9) { throw "Incomplete multiplier: $($match.Value)" }
    }
    if ($Modes[$i] -eq 5 -and $sessionLog -notmatch 'DLSSNR STATUS: Feature=18 .*created=true') { throw 'DLSSNR combination did not initialize' }
    $last = $sdkMatches[$sdkMatches.Count - 1]
    if ([int]$last.Groups[2].Value -lt 120) { throw "Fewer than 120 submissions for ${multiplier}x; startup samples alone do not validate steady output" }
    $motionFrames = $null
    if ($OpticalFlow -ne 'None') {
        $motionSamples = [regex]::Matches($sessionLog, 'XeSSFG SDK output: [^\r\n]*motionFrames=(\d+)')
        if (!$motionSamples.Count) { throw 'No external-motion submission observations' }
        $motionFrames = [int]$motionSamples[-1].Groups[1].Value
        if ($motionFrames -lt [int]$last.Groups[2].Value * 0.9) { throw 'External motion was missing or reset for too many submissions' }
    }
    $startup = $null
    if ($StartupTrace) {
        $timed = [regex]::Matches($sessionLog, '(?m)^(.{23})[^\r\n]*XeSSFG SDK output: requested=\d+x frames=\d+ submissions=(\d+) partialBursts=\d+ lastFrames=(\d+)')
        $first = [datetime]::ParseExact($timed[0].Groups[1].Value, 'yyyy-MM-dd HH:mm:ss.fff', [cultureinfo]::InvariantCulture)
        $startup = @{}
        foreach ($sample in $timed) {
            $stamp = [datetime]::ParseExact($sample.Groups[1].Value, 'yyyy-MM-dd HH:mm:ss.fff', [cultureinfo]::InvariantCulture)
            $inputCount = [int]$sample.Groups[2].Value
            if (!$startup.ContainsKey('firstFullMs') -and [int]$sample.Groups[3].Value -eq $multiplier) {
                $startup.firstFullMs = ($stamp - $first).TotalMilliseconds
            }
            if ($inputCount -in @(120,240)) { $startup["first${inputCount}Seconds"] = ($stamp - $first).TotalSeconds }
        }
        $startup.firstFrameRTSS = $sessionLog -match 'XeSSFG startup: frame=1 rtss=true'
    }
    $observations += @{mode=$Modes[$i]; multiplier=$multiplier; frames=[int]$last.Groups[1].Value;
        submissions=[int]$last.Groups[2].Value; partialBursts=[int]$last.Groups[3].Value; motionFrames=$motionFrames; startup=$startup}
}
if ($log -match 'XeSSFG disabled|XeSSFG.*failed|compatibility.*poison') { throw 'XeSSFG runtime failure in capture log' }
if ($log -match 'Frame Guidance NVOF failed|NVOF input format mismatch|XeSSFG motion frame mismatch') { throw 'Optical-flow failure in capture log' }
@{runtime=$runtime; rtssInjected=$rtssInjected; observations=$observations; seconds=$Seconds;
    startupTrace=[bool]$StartupTrace; fullscreen=[bool]$Fullscreen; opticalFlow=$OpticalFlow; nvidiaQuality=$NvidiaQuality;
    executableSha256=(Get-FileHash -LiteralPath $appPath).Hash} | ConvertTo-Json -Depth 6 |
    Set-Content -LiteralPath (Join-Path $output 'capture-verification.json') -Encoding utf8
Write-Output "Actual WGC + $OpticalFlow capture passed for modes $($Modes -join '/'), where 5 means DLSSNR + 4x. SDK counts are not display events."

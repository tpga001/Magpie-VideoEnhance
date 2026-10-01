#Requires -Version 7.0
param(
    [Parameter(Mandatory)][string]$RuntimeDirectory,
    [ValidateRange(5, 120)][int]$Seconds = 12,
    [string]$TargetDisplay = '\\.\DISPLAY1'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path $PSScriptRoot -Parent
$workspace = Split-Path $repo -Parent
$edgePath = 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe'

if (!(Test-Path -LiteralPath $edgePath -PathType Leaf)) {
    throw "Microsoft Edge was not found: $edgePath"
}
if (Get-Process Magpie -ErrorAction SilentlyContinue) {
    throw 'Close the regular Magpie instance before the isolated browser smoke test.'
}

$manifestPath = Join-Path $RuntimeDirectory 'build-manifest.json'
if (!(Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
    throw "Missing package manifest: $manifestPath"
}

$output = Join-Path $workspace ('.tools/browser-4k60/smoke-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$runtime = Join-Path $output 'runtime'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null

$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
foreach ($file in $manifest.files) {
    if ([IO.Path]::GetExtension($file.path) -in @('.pdb', '.map')) { continue }
    $source = Join-Path $RuntimeDirectory $file.path
    $target = Join-Path $runtime $file.path
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $source -Destination $target
}

$effects = @(
    @{
        name = 'FrameRate_Filter'
        parameters = @{ frameRateMode = 1; targetFrameRate = 30 }
    },
    @{
        name = 'RTXVideo\RTXVideo_VSR'
        scalingType = 1
        scale = @{ x = 1; y = 1 }
        parameters = @{ strength = 1 }
    },
    @{
        name = 'XeSSFG\XeSS_FrameGeneration'
        parameters = @{
            multiplier = 2
            duplicateFrameFiltering = 1
            opticalFlowMethod = 1
            amdOpticalFlowMode = 1
            nvidiaOpticalFlowQuality = 2
        }
    }
)
$config = @{
    autoCheckForUpdates = $false
    showNotifyIcon = $true
    alwaysRunAsAdmin = $false
    allowScalingMaximized = $true
    frontEdgeSync = $true
    frontEdgeSyncFrameRate = 60
    frameSyncMode = 0
    vrr = $false
    debugMode = $true
    duplicateFrameDetectionMode = 1
    experimentalXeSSFGSettingsVersion = 1
    experimentalOpticalFlowDefaultsVersion = 1
    scalingModes = @(@{ name = 'Browser Video 30fps to Display-Native 60fps'; effects = $effects })
    profiles = @(@{
        scalingMode = 0
        captureMethod = 0
        initialWindowedScaleFactor = 0
        parameterFocusSwitching = $false
        enableHdrCompatibility = $false
        frameRateLimiterEnabled = $true
        maxFrameRate = 30
    })
}
$configDir = Join-Path $runtime 'config/v4e'
New-Item -ItemType Directory -Path $configDir -Force | Out-Null
$config | ConvertTo-Json -Depth 12 |
    Set-Content -LiteralPath (Join-Path $configDir 'config.json') -Encoding utf8

Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;

public static class Browser4K60Native {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr lParam);
    public delegate bool EnumMonitorsProc(IntPtr monitor, IntPtr hdc, IntPtr rect, IntPtr data);
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct MonitorInfoEx {
        public int Size;
        public Rect Monitor;
        public Rect Work;
        public uint Flags;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst=32)] public string Device;
    }
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool EnumDisplayMonitors(IntPtr hdc, IntPtr clip, EnumMonitorsProc cb, IntPtr data);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern bool GetMonitorInfo(IntPtr monitor, ref MonitorInfoEx info);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr hwnd, int command);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr SetFocus(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr hwnd, bool altTab);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string title);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr GetProp(IntPtr hwnd, string name);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr hwnd, IntPtr after, int x, int y, int width, int height, uint flags);

    private static IntPtr FindEdge(string runToken, bool readyOnly) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((hwnd, _) => {
            if (!IsWindowVisible(hwnd)) return true;
            var cls = new StringBuilder(128);
            var title = new StringBuilder(512);
            GetClassName(hwnd, cls, cls.Capacity);
            GetWindowText(hwnd, title, title.Capacity);
            string value = title.ToString();
            if (cls.ToString() == "Chrome_WidgetWin_1" &&
                value.Contains("Magpie Browser 4K60 Lab " + runToken) &&
                (!readyOnly || value.Contains("[READY]"))) {
                found = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }
    public static IntPtr FindTestEdge(string runToken) { return FindEdge(runToken, false); }
    public static IntPtr FindReadyEdge(string runToken) { return FindEdge(runToken, true); }

    public static bool Focus(IntPtr hwnd) {
        ShowWindowAsync(hwnd, 9);
        keybd_event(0x12, 0, 0, UIntPtr.Zero);
        keybd_event(0x12, 0, 2, UIntPtr.Zero);
        BringWindowToTop(hwnd);
        SetForegroundWindow(hwnd);
        SwitchToThisWindow(hwnd, true);
        SetFocus(hwnd);
        return GetForegroundWindow() == hwnd;
    }

    public static Rect FindMonitorRect(string device) {
        SetThreadDpiAwarenessContext(new IntPtr(-4));
        Rect result = new Rect();
        bool found = false;
        EnumDisplayMonitors(IntPtr.Zero, IntPtr.Zero, (monitor, _, _, _) => {
            var info = new MonitorInfoEx { Size = Marshal.SizeOf<MonitorInfoEx>() };
            if (GetMonitorInfo(monitor, ref info) &&
                string.Equals(info.Device, device, StringComparison.OrdinalIgnoreCase)) {
                result = info.Monitor;
                found = true;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        if (!found) throw new InvalidOperationException("Display not found: " + device);
        return result;
    }

    public static string ResizeClientPhysical(IntPtr hwnd, int x, int y, int width, int height) {
        SetThreadDpiAwarenessContext(new IntPtr(-4));
        Rect window, client;
        // Moving a Chromium window between monitors can trigger WM_DPICHANGED
        // after the first SetWindowPos. Recompute the non-client extent on the
        // destination monitor so the final client area is expressed in physical pixels.
        for (int attempt = 0; attempt < 3; ++attempt) {
            GetWindowRect(hwnd, out window);
            GetClientRect(hwnd, out client);
            int nonClientWidth = (window.Right - window.Left) - (client.Right - client.Left);
            int nonClientHeight = (window.Bottom - window.Top) - (client.Bottom - client.Top);
            SetWindowPos(hwnd, IntPtr.Zero, x, y,
                width + nonClientWidth, height + nonClientHeight, 0x0004);
            System.Threading.Thread.Sleep(500);
        }
        GetClientRect(hwnd, out client);
        return $"{client.Right - client.Left}x{client.Bottom - client.Top}";
    }
}
'@

$targetMonitor = [Browser4K60Native]::FindMonitorRect($TargetDisplay)
$monitorWidth = $targetMonitor.Right - $targetMonitor.Left
$monitorHeight = $targetMonitor.Bottom - $targetMonitor.Top
$sourceWidth = [Math]::Min(1920, $monitorWidth)
$sourceHeight = [int][Math]::Round($sourceWidth * 9.0 / 16.0)
$fitScale = [Math]::Min($monitorWidth / [double]$sourceWidth, $monitorHeight / [double]$sourceHeight)
$expectedOutputWidth = [int][Math]::Round($sourceWidth * $fitScale)
$expectedOutputHeight = [int][Math]::Round($sourceHeight * $fitScale)
$expectVsr = $expectedOutputWidth -gt $sourceWidth -or $expectedOutputHeight -gt $sourceHeight

$fixturePath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'browser-video-1080p30.html')).Path
$runToken = Split-Path $output -Leaf
$fixtureUri = [Uri]::new($fixturePath).AbsoluteUri + '?run=' + [Uri]::EscapeDataString($runToken)
$edgeProfile = Join-Path $output 'edge-profile'
$edgeArgs = @(
    "--user-data-dir=$edgeProfile",
    "--app=$fixtureUri",
    '--no-first-run',
    '--no-default-browser-check',
    '--disable-first-run-ui',
    '--disable-background-mode',
    '--disable-features=msEdgeSidebarV2,msHubApps,msCopilot,msEdgeFirstRunExperience',
    '--window-size=1280,720'
)
$edgeLauncher = Start-Process -FilePath $edgePath -ArgumentList $edgeArgs -PassThru
$browser = [IntPtr]::Zero
for ($i = 0; $i -lt 20 -and $browser -eq [IntPtr]::Zero; ++$i) {
    Start-Sleep -Milliseconds 500
    $browser = [Browser4K60Native]::FindTestEdge($runToken)
}
if ($browser -eq [IntPtr]::Zero) {
    throw 'The dedicated Edge test window was not created.'
}
$browserClientHeight = $sourceHeight + 2
$browserClient = [Browser4K60Native]::ResizeClientPhysical(
    $browser, $targetMonitor.Left + 5, $targetMonitor.Top + 5,
    $sourceWidth, $browserClientHeight)
if ($browserClient -ne "${sourceWidth}x${browserClientHeight}") {
    throw "Failed to size the browser client area to ${sourceWidth}x${browserClientHeight}: $browserClient"
}
[void][Browser4K60Native]::Focus($browser)
for ($i = 0; $i -lt 40; ++$i) {
    Start-Sleep -Milliseconds 500
    $readyBrowser = [Browser4K60Native]::FindReadyEdge($runToken)
    if ($readyBrowser -ne [IntPtr]::Zero) {
        $browser = $readyBrowser
        break
    }
    if ($i % 4 -eq 0) { [void][Browser4K60Native]::Focus($browser) }
}
if ([Browser4K60Native]::FindReadyEdge($runToken) -eq [IntPtr]::Zero) {
    [void][Browser4K60Native]::PostMessage($browser, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    throw 'The dedicated Edge test window did not reach READY state.'
}

$appPath = Join-Path $runtime 'MagpieBrowser4K60SmokeApp.exe'
Copy-Item -LiteralPath (Join-Path $runtime 'Magpie.exe') -Destination $appPath
$app = Start-Process -FilePath $appPath -ArgumentList '-t' -WorkingDirectory $runtime `
    -WindowStyle Hidden -PassThru

try {
    Start-Sleep -Seconds 3
    $focused = [Browser4K60Native]::Focus($browser)
    Start-Sleep -Milliseconds 500
    $hotkey = [Browser4K60Native]::FindWindow('Magpie_Hotkey', $null)
    if ($hotkey -eq [IntPtr]::Zero) { throw 'The isolated Magpie hotkey window was not found.' }
    [void][Browser4K60Native]::PostMessage($hotkey, 0x0312, [IntPtr]::Zero, [IntPtr]::Zero)

    $scaling = [IntPtr]::Zero
    for ($i = 0; $i -lt 30 -and $scaling -eq [IntPtr]::Zero; ++$i) {
        Start-Sleep -Milliseconds 500
        $scaling = [Browser4K60Native]::FindWindow(
            'Window_Magpie_967EB565-6F73-4E94-AE53-00CC42592A22', $null)
    }
    if ($scaling -eq [IntPtr]::Zero) {
        throw "Magpie did not create a scaling window (foreground activation=$focused)."
    }

    $rects = @{}
    foreach ($name in @('SrcLeft','SrcTop','SrcRight','SrcBottom','DestLeft','DestTop','DestRight','DestBottom')) {
        $rects[$name] = [Browser4K60Native]::GetProp($scaling, "Magpie.$name").ToInt64()
    }
    Start-Sleep -Seconds $Seconds
} finally {
    $notify = [Browser4K60Native]::FindWindow('Magpie_NotifyIcon', $null)
    [uint32]$owner = 0
    if ($notify -ne [IntPtr]::Zero) {
        [void][Browser4K60Native]::GetWindowThreadProcessId($notify, [ref]$owner)
    }
    if ($owner -eq $app.Id) {
        [void][Browser4K60Native]::PostMessage(
            $notify,
            [Browser4K60Native]::RegisterWindowMessage('WM_MAGPIE_QUIT'),
            [IntPtr]::Zero,
            [IntPtr]::Zero)
    }
    if (!$app.WaitForExit(15000)) {
        throw "Isolated Magpie did not exit: PID $($app.Id)"
    }
    if ($browser -ne [IntPtr]::Zero) {
        [void][Browser4K60Native]::PostMessage(
            $browser, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
    }
}

$logFiles = @(Get-ChildItem -LiteralPath (Join-Path $runtime 'logs') -Filter 'magpie*.log' |
    Sort-Object LastWriteTime)
if (!$logFiles.Count) { throw 'The isolated Magpie process produced no log.' }
$log = ($logFiles | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join "`n"
$vsr = [regex]::Match($log, 'RTX Video VSR initialized: quality=(\d+), (\d+)x(\d+)')
$vsrBypass = [regex]::Match($log, 'RTX Video VSR bypassed: output does not upscale (\d+)x(\d+) -> (\d+)x(\d+)')
$xess = [regex]::Match($log, 'XeSSFG initialized: (\d+)x(\d+), multiplier=(\d+)x')
$sdk = [regex]::Matches($log, 'XeSSFG SDK output: requested=2x frames=(\d+) submissions=(\d+).*?lastFrames=(\d+) lastFGResult=(-?\d+)')
$pacing = [regex]::Match($log, 'Frame sync effective base target: 30\.000 FPS \(existingLimit=30\.000\)')
if ($expectVsr -and !$vsr.Success) { throw 'RTX Video VSR did not initialize; inspect the isolated runtime log.' }
if (!$expectVsr -and !$vsrBypass.Success) { throw 'RTX Video VSR was not bypassed at display-native resolution.' }
if (!$xess.Success) { throw 'XeSSFG did not initialize; inspect the isolated runtime log.' }
if (!$pacing.Success) { throw 'The browser-scoped 30 FPS base limit was not applied.' }
if (!$sdk.Count -or !($sdk | Where-Object { [int]$_.Groups[3].Value -eq 2 -and [int]$_.Groups[4].Value -eq 0 })) {
    throw 'XeSSFG did not report a successful two-frame output burst.'
}
if ($expectVsr -and ($vsr.Groups[1].Value -ne '2' -or
    [int]$vsr.Groups[2].Value -ne $sourceWidth -or
    [int]$vsr.Groups[3].Value -ne $sourceHeight)) {
    throw "Unexpected RTX Video VSR quality or input dimensions: $($vsr.Value)"
}
$verifiedSourceWidth = if ($expectVsr) { [int]$vsr.Groups[2].Value } else { [int]$vsrBypass.Groups[1].Value }
$verifiedSourceHeight = if ($expectVsr) { [int]$vsr.Groups[3].Value } else { [int]$vsrBypass.Groups[2].Value }
$verifiedOutputWidth = if ($expectVsr) { $expectedOutputWidth } else { [int]$vsrBypass.Groups[3].Value }
$verifiedOutputHeight = if ($expectVsr) { $expectedOutputHeight } else { [int]$vsrBypass.Groups[4].Value }
if (!$expectVsr -and ($verifiedSourceWidth -ne $verifiedOutputWidth -or
    $verifiedSourceHeight -ne $verifiedOutputHeight -or
    $verifiedOutputWidth -gt $monitorWidth -or $verifiedOutputHeight -gt $monitorHeight)) {
    throw "Unexpected RTX Video VSR bypass dimensions: $($vsrBypass.Value)"
}
if ([int]$xess.Groups[1].Value -ne $monitorWidth -or
    [int]$xess.Groups[2].Value -ne $monitorHeight -or $xess.Groups[3].Value -ne '2') {
    throw "Unexpected XeSSFG dimensions or multiplier: $($xess.Value)"
}

$lastSdk = $sdk[$sdk.Count - 1]
$verification = [ordered]@{
    passed = $true
    runtime = $runtime
    browserFixture = $fixturePath
    browserClient = $browserClient
    targetDisplay = $TargetDisplay
    targetMonitor = @{ width = $monitorWidth; height = $monitorHeight; left = $targetMonitor.Left; top = $targetMonitor.Top }
    media = @{ width = $verifiedSourceWidth; height = $verifiedSourceHeight; fps = 30 }
    superResolution = @{
        engine = if ($expectVsr) { 'RTX Video VSR' } else { 'Bypassed at display-native resolution' }
        qualityLevel = if ($expectVsr) { [int]$vsr.Groups[1].Value } else { $null }
        input = "${verifiedSourceWidth}x${verifiedSourceHeight}"
        output = "${verifiedOutputWidth}x${verifiedOutputHeight}"
    }
    frameGeneration = @{
        engine = 'XeSSFG'
        resolution = "$($xess.Groups[1].Value)x$($xess.Groups[2].Value)"
        multiplier = [int]$xess.Groups[3].Value
        requested = 2
        lastFrames = [int]$lastSdk.Groups[3].Value
        lastResult = [int]$lastSdk.Groups[4].Value
        totalFrames = [int64]$lastSdk.Groups[1].Value
        submissions = [int64]$lastSdk.Groups[2].Value
    }
    framePacing = @{
        globalTargetFps = 60
        browserProfileLimitFps = 30
        effectiveBaseFps = 30
    }
    targetFps = 60
    scalingWindow = $rects
    foregroundActivation = $focused
    logs = @($logFiles.FullName)
}
$verification | ConvertTo-Json -Depth 8 |
    Set-Content -LiteralPath (Join-Path $output 'verification.json') -Encoding utf8
$verification | ConvertTo-Json -Depth 8

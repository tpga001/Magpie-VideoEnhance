#Requires -Version 7.0
param(
    [string]$ConfigPath = (Join-Path $env:LOCALAPPDATA 'Magpie\config\v4e\config.json'),
    [string]$RuntimeDirectory = $PSScriptRoot,
    [ValidateRange(1, 10)][int]$CountdownSeconds = 3,
    [long]$WindowHandle = 0,
    [string]$Region,
    [switch]$NoRestart,
    [switch]$NoAutoStart
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$typeReferences = @([AppDomain]::CurrentDomain.GetAssemblies() |
    Where-Object Location | ForEach-Object Location)
$formsPrimitives = Join-Path (Split-Path ([Windows.Forms.Form].Assembly.Location) -Parent) `
    'System.Windows.Forms.Primitives.dll'
if (Test-Path -LiteralPath $formsPrimitives) { $typeReferences += $formsPrimitives }

Add-Type -ReferencedAssemblies $typeReferences -TypeDefinition @'
using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

public static class BrowserVideoRegionNative {
    [StructLayout(LayoutKind.Sequential)] public struct Rect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct Point { public int X, Y; }
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr context);
    [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr hwnd, uint flags);
    [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr hwnd, out Rect rect);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr hwnd, ref Point point);
    [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr hwnd);
    [DllImport("dwmapi.dll")] public static extern int DwmGetWindowAttribute(IntPtr hwnd, int attribute, out Rect rect, int size);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    public static extern int GetClassName(IntPtr hwnd, StringBuilder text, int count);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd, out uint pid);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string value);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint msg, IntPtr wp, IntPtr lp);
    [DllImport("user32.dll")] public static extern bool ShowWindowAsync(IntPtr hwnd, int command);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
}

public sealed class BrowserVideoSelectorForm : Form {
    private System.Drawing.Point start;
    private Rectangle selection;
    private bool dragging;
    public Rectangle Selection { get { return selection; } }

    public BrowserVideoSelectorForm(Rectangle bounds) {
        Bounds = bounds;
        FormBorderStyle = FormBorderStyle.None;
        StartPosition = FormStartPosition.Manual;
        TopMost = true;
        ShowInTaskbar = false;
        BackColor = Color.Black;
        Opacity = 0.28;
        Cursor = Cursors.Cross;
        KeyPreview = true;
        DoubleBuffered = true;
        MouseDown += OnMouseDown;
        MouseMove += OnMouseMove;
        MouseUp += OnMouseUp;
        KeyDown += OnKeyDown;
        Paint += OnPaint;
    }

    private void OnMouseDown(object sender, MouseEventArgs e) {
        if (e.Button != MouseButtons.Left) return;
        start = e.Location;
        selection = Rectangle.Empty;
        dragging = true;
        Invalidate();
    }

    private void OnMouseMove(object sender, MouseEventArgs e) {
        if (!dragging) return;
        int left = Math.Max(0, Math.Min(start.X, e.X));
        int top = Math.Max(0, Math.Min(start.Y, e.Y));
        int right = Math.Min(ClientSize.Width, Math.Max(start.X, e.X));
        int bottom = Math.Min(ClientSize.Height, Math.Max(start.Y, e.Y));
        selection = Rectangle.FromLTRB(left, top, right, bottom);
        Invalidate();
    }

    private void OnMouseUp(object sender, MouseEventArgs e) {
        if (e.Button != MouseButtons.Left || !dragging) return;
        dragging = false;
        if (selection.Width >= 64 && selection.Height >= 64) {
            DialogResult = DialogResult.OK;
            Close();
        }
    }

    private void OnKeyDown(object sender, KeyEventArgs e) {
        if (e.KeyCode == Keys.Escape) {
            DialogResult = DialogResult.Cancel;
            Close();
        }
    }

    private void OnPaint(object sender, PaintEventArgs e) {
        using (var shade = new SolidBrush(Color.FromArgb(120, 20, 180, 255))) {
            e.Graphics.FillRectangle(shade, selection);
        }
        if (!selection.IsEmpty) {
            using (var pen = new Pen(Color.White, 3)) e.Graphics.DrawRectangle(pen, selection);
        }
        using (var font = new Font("Microsoft YaHei UI", 14, FontStyle.Bold))
        using (var brush = new SolidBrush(Color.White)) {
            e.Graphics.DrawString("拖动框选视频区域，Esc 取消", font, brush, new PointF(24, 20));
        }
    }
}

public static class BrowserVideoSelector {
    public static Rectangle Select(Rectangle bounds) {
        Application.EnableVisualStyles();
        using (var form = new BrowserVideoSelectorForm(bounds)) {
            return form.ShowDialog() == DialogResult.OK ? form.Selection : Rectangle.Empty;
        }
    }
}
'@

function Set-JsonProperty([object]$Object, [string]$Name, [object]$Value) {
    $Object | Add-Member -NotePropertyName $Name -NotePropertyValue $Value -Force
}

# Use physical pixels so the drag rectangle and Magpie's cropping values match
# on displays with 125%, 150%, or mixed per-monitor DPI scaling.
[void][BrowserVideoRegionNative]::SetThreadDpiAwarenessContext([IntPtr]::new(-4))

if (!(Test-Path -LiteralPath $ConfigPath -PathType Leaf)) {
    throw "Magpie configuration was not found: $ConfigPath"
}

if ($WindowHandle) {
    $target = [IntPtr]::new($WindowHandle)
} else {
    Write-Host "请在 $CountdownSeconds 秒内切换到正在播放视频的浏览器窗口。"
    for ($remaining = $CountdownSeconds; $remaining -gt 0; --$remaining) {
        Write-Host "$remaining..."
        Start-Sleep -Seconds 1
    }
    $target = [BrowserVideoRegionNative]::GetForegroundWindow()
}
$target = [BrowserVideoRegionNative]::GetAncestor($target, 2)
if ($target -eq [IntPtr]::Zero -or ![BrowserVideoRegionNative]::IsWindow($target)) {
    throw 'No valid browser window was selected.'
}

$targetPid = [uint32]0
[void][BrowserVideoRegionNative]::GetWindowThreadProcessId($target, [ref]$targetPid)
$process = Get-Process -Id $targetPid -ErrorAction Stop
$processPath = $process.Path
$browserNames = @('msedge.exe', 'chrome.exe', 'brave.exe', 'vivaldi.exe', 'opera.exe', 'firefox.exe')
if ([IO.Path]::GetFileName($processPath).ToLowerInvariant() -notin $browserNames) {
    throw "The selected window is not a supported browser: $processPath"
}

$classBuffer = [Text.StringBuilder]::new(256)
[void][BrowserVideoRegionNative]::GetClassName($target, $classBuffer, $classBuffer.Capacity)
$className = $classBuffer.ToString()
if ($className -like 'Chrome_WidgetWin_*') { $className = 'Chrome_WidgetWin_1' }

$client = [BrowserVideoRegionNative+Rect]::new()
if (![BrowserVideoRegionNative]::GetClientRect($target, [ref]$client)) {
    throw 'Failed to read the browser client rectangle.'
}
$origin = [BrowserVideoRegionNative+Point]::new()
if (![BrowserVideoRegionNative]::ClientToScreen($target, [ref]$origin)) {
    throw 'Failed to locate the browser client rectangle on screen.'
}
$clientWidth = $client.Right - $client.Left
$clientHeight = $client.Bottom - $client.Top
if ($clientWidth -lt 64 -or $clientHeight -lt 64) {
    throw 'The selected browser window is too small.'
}

# Match Magpie's source rectangle for standard framed browser windows. Magpie
# captures the DWM extended frame inset by the native border and clips its top
# edge to the client area. This differs from GetClientRect by two physical
# pixels at 150% DPI on current Chromium builds.
$sourceLeft = $origin.X
$sourceTop = $origin.Y
$sourceRight = $origin.X + $clientWidth
$sourceBottom = $origin.Y + $clientHeight
$frame = [BrowserVideoRegionNative+Rect]::new()
$frameResult = [BrowserVideoRegionNative]::DwmGetWindowAttribute(
    $target, 9, [ref]$frame, [Runtime.InteropServices.Marshal]::SizeOf($frame))
if ($frameResult -eq 0) {
    $dpi = [BrowserVideoRegionNative]::GetDpiForWindow($target)
    if (!$dpi) { $dpi = 96 }
    $border = [int][Math]::Floor(($dpi + 48) / 96.0)
    $candidateLeft = $frame.Left + $border
    $candidateTop = [Math]::Max($frame.Top + $border, $origin.Y)
    $candidateRight = $frame.Right - $border
    $candidateBottom = $frame.Bottom - $border
    if ($candidateRight - $candidateLeft -ge 64 -and
        $candidateBottom - $candidateTop -ge 64) {
        $sourceLeft = $candidateLeft
        $sourceTop = $candidateTop
        $sourceRight = $candidateRight
        $sourceBottom = $candidateBottom
    }
}

if ($Region) {
    $regionValues = @($Region.Split(',') | ForEach-Object {
        $parsed = 0
        if (![int]::TryParse($_.Trim(), [ref]$parsed)) {
            throw 'Region must contain integer x, y, width, height values.'
        }
        $parsed
    })
    if ($regionValues.Count -ne 4) { throw 'Region must contain x, y, width, height.' }
    $selection = [Drawing.Rectangle]::new(
        $regionValues[0], $regionValues[1], $regionValues[2], $regionValues[3])
} else {
    $bounds = [Drawing.Rectangle]::new($origin.X, $origin.Y, $clientWidth, $clientHeight)
    $selection = [BrowserVideoSelector]::Select($bounds)
}
if ($selection.Width -lt 64 -or $selection.Height -lt 64 -or
    $selection.Left -lt 0 -or $selection.Top -lt 0 -or
    $selection.Right -gt $clientWidth -or $selection.Bottom -gt $clientHeight) {
    throw 'Video region selection was cancelled or is outside the browser client area.'
}

$configFile = (Resolve-Path -LiteralPath $ConfigPath).Path
$config = Get-Content -LiteralPath $configFile -Raw | ConvertFrom-Json
$modeName = '浏览器视频 30fps → 当前屏幕原生60fps'
$effects = @(
    [ordered]@{ name = 'FrameRate_Filter'; parameters = [ordered]@{ frameRateMode = 1; targetFrameRate = 30 } },
    [ordered]@{ name = 'RTXVideo\RTXVideo_VSR'; scalingType = 1; scale = [ordered]@{ x = 1.0; y = 1.0 }; parameters = [ordered]@{ strength = 3.0 } },
    [ordered]@{ name = 'XeSSFG\XeSS_FrameGeneration'; parameters = [ordered]@{ multiplier = 2.0; duplicateFrameFiltering = 1.0; opticalFlowMethod = 1.0; amdOpticalFlowMode = 1.0; nvidiaOpticalFlowQuality = 2.0 } }
)
$modes = @($config.scalingModes)
$modeIndex = -1
for ($i = 0; $i -lt $modes.Count; ++$i) {
    if ($modes[$i].name -eq $modeName) {
        $modeIndex = $i
        break
    }
}
if ($modeIndex -lt 0) {
    $modeIndex = $modes.Count
    $modes += [pscustomobject][ordered]@{ name = $modeName; effects = $effects }
} else {
    $modes[$modeIndex].effects = $effects
}
$vsrOnlyName = '浏览器视频 → 当前屏幕原生（仅RTX VSR）'
$vsrOnlyEffects = @(
    [ordered]@{ name = 'RTXVideo\RTXVideo_VSR'; scalingType = 1; scale = [ordered]@{ x = 1.0; y = 1.0 }; parameters = [ordered]@{ strength = 3.0 } }
)
$vsrOnlyIndex = -1
for ($i = 0; $i -lt $modes.Count; ++$i) {
    if ($modes[$i].name -eq $vsrOnlyName) {
        $vsrOnlyIndex = $i
        $modes[$i].effects = $vsrOnlyEffects
        break
    }
}
if ($vsrOnlyIndex -lt 0) {
    $vsrOnlyIndex = $modes.Count
    $modes += [pscustomobject][ordered]@{ name = $vsrOnlyName; effects = $vsrOnlyEffects }
}
$config.scalingModes = $modes

$selectedLeft = [Math]::Max($origin.X + $selection.Left, $sourceLeft)
$selectedTop = [Math]::Max($origin.Y + $selection.Top, $sourceTop)
$selectedRight = [Math]::Min($origin.X + $selection.Right, $sourceRight)
$selectedBottom = [Math]::Min($origin.Y + $selection.Bottom, $sourceBottom)
if ($selectedRight - $selectedLeft -lt 64 -or $selectedBottom - $selectedTop -lt 64) {
    throw 'The selected video region falls outside Magpie''s capturable source rectangle.'
}
$crop = [ordered]@{
    left = [double]($selectedLeft - $sourceLeft)
    top = [double]($selectedTop - $sourceTop)
    right = [double]($sourceRight - $selectedRight)
    bottom = [double]($sourceBottom - $selectedBottom)
}
$profiles = @($config.profiles)
$profile = $profiles | Where-Object {
    $packagedProperty = $_.PSObject.Properties['packaged']
    $pathProperty = $_.PSObject.Properties['pathRule']
    $classProperty = $_.PSObject.Properties['classNameRule']
    $packagedProperty -and !$packagedProperty.Value -and
        $pathProperty -and $pathProperty.Value -eq $processPath -and
        $classProperty -and $classProperty.Value -eq $className
} | Select-Object -First 1
if (!$profile) {
    $profile = [pscustomobject][ordered]@{
        name = "$($process.ProcessName) 视频区域·当前屏幕原生60fps"
        packaged = $false
        pathRule = $processPath
        classNameRule = $className
        launcherPath = ''
        autoScale = 0
        launchParameters = ''
        parameterFocusSwitching = $false
        scalingMode = $modeIndex
        captureMethod = 0
        multiMonitorUsage = 0
        initialWindowedScaleFactor = 0
        customInitialWindowedScaleFactor = 1.25
        graphicsCardId = [ordered]@{ idx = -1; vendorId = 0; deviceId = 0 }
        frameRateLimiterEnabled = $true
        maxFrameRate = 30.0
        '3DGameMode' = $false
        captureTitleBar = $false
        adjustCursorSpeed = $false
        disableDirectFlip = $false
        enableHdrCompatibility = $false
        cursorScaling = 2
        customCursorScaling = 1.0
        cursorInterpolationMode = 0
        autoHideCursorEnabled = $false
        autoHideCursorDelay = 3.0
        croppingEnabled = $true
        cropping = $crop
        destAlignment = 4
    }
    $profiles += $profile
} else {
    Set-JsonProperty $profile 'scalingMode' $modeIndex
    Set-JsonProperty $profile 'autoScale' 0
    Set-JsonProperty $profile 'frameRateLimiterEnabled' $true
    Set-JsonProperty $profile 'maxFrameRate' 30.0
    Set-JsonProperty $profile 'captureTitleBar' $false
    Set-JsonProperty $profile 'adjustCursorSpeed' $false
    Set-JsonProperty $profile 'croppingEnabled' $true
    Set-JsonProperty $profile 'cropping' $crop
    Set-JsonProperty $profile 'multiMonitorUsage' 0
}
$config.profiles = $profiles

$runningMagpie = @(Get-Process Magpie -ErrorAction SilentlyContinue)
$restartPath = $null
if (!$NoRestart) {
    if ($runningMagpie.Count) {
        $restartPath = $runningMagpie[0].Path
        $quitMessage = [BrowserVideoRegionNative]::RegisterWindowMessage('WM_MAGPIE_QUIT')
        [void][BrowserVideoRegionNative]::PostMessage([IntPtr]0xffff, $quitMessage, [IntPtr]::Zero, [IntPtr]::Zero)
        $deadline = (Get-Date).AddSeconds(5)
        while ((Get-Process Magpie -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) {
            Start-Sleep -Milliseconds 200
        }
        Get-Process Magpie -ErrorAction SilentlyContinue | Stop-Process -Force
        Start-Sleep -Milliseconds 500
    }
    if (!$restartPath) { $restartPath = Join-Path $RuntimeDirectory 'Magpie.exe' }
}

$configDir = Split-Path $configFile -Parent
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = Join-Path $configDir "config.before-video-region-$stamp.json"
Copy-Item -LiteralPath $configFile -Destination $backup
$temp = Join-Path $configDir "config.video-region-$stamp.tmp"
$config | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $temp -Encoding utf8NoBOM
$roundTrip = Get-Content -LiteralPath $temp -Raw | ConvertFrom-Json
$savedProfile = @($roundTrip.profiles | Where-Object {
    $pathProperty = $_.PSObject.Properties['pathRule']
    $classProperty = $_.PSObject.Properties['classNameRule']
    $pathProperty -and $pathProperty.Value -eq $processPath -and
        $classProperty -and $classProperty.Value -eq $className
})
if ($savedProfile.Count -ne 1 -or !$savedProfile[0].croppingEnabled) {
    throw 'The generated video-region profile failed round-trip validation.'
}
[IO.File]::Move($temp, $configFile, $true)

if (!$NoRestart) {
    if (!(Test-Path -LiteralPath $restartPath -PathType Leaf)) {
        throw "Magpie runtime was not found for restart: $restartPath"
    }
    Start-Process -FilePath $restartPath -ArgumentList '-t' -WorkingDirectory (Split-Path $restartPath -Parent)
}

$effectsStarted = $false
if (!$NoRestart -and !$NoAutoStart) {
    $deadline = (Get-Date).AddSeconds(10)
    while (!(Get-Process Magpie -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 200
    }
    Start-Sleep -Seconds 2
    [void][BrowserVideoRegionNative]::ShowWindowAsync($target, 9)
    [BrowserVideoRegionNative]::keybd_event(0x12, 0, 0, [UIntPtr]::Zero)
    [BrowserVideoRegionNative]::keybd_event(0x12, 0, 2, [UIntPtr]::Zero)
    [void][BrowserVideoRegionNative]::BringWindowToTop($target)
    [void][BrowserVideoRegionNative]::SetForegroundWindow($target)
    Start-Sleep -Milliseconds 300

    $shortcut = [uint32]$config.shortcuts.scale
    if ($shortcut -gt 0xfff -or ($shortcut -band 0xff) -eq 0) {
        throw 'The configured fullscreen effect shortcut is invalid.'
    }
    $keys = [Collections.Generic.List[byte]]::new()
    if ($shortcut -band 0x100) { $keys.Add(0x5b) }
    if ($shortcut -band 0x200) { $keys.Add(0x11) }
    if ($shortcut -band 0x400) { $keys.Add(0x12) }
    if ($shortcut -band 0x800) { $keys.Add(0x10) }
    $keys.Add([byte]($shortcut -band 0xff))
    foreach ($key in $keys) {
        [BrowserVideoRegionNative]::keybd_event($key, 0, 0, [UIntPtr]::Zero)
    }
    Start-Sleep -Milliseconds 80
    for ($i = $keys.Count - 1; $i -ge 0; --$i) {
        [BrowserVideoRegionNative]::keybd_event($keys[$i], 0, 2, [UIntPtr]::Zero)
    }
    $effectsStarted = $true
}

[ordered]@{
    configured = $true
    browser = $processPath
    className = $className
    clientSize = "${clientWidth}x${clientHeight}"
    sourceBounds = [ordered]@{
        left = $sourceLeft; top = $sourceTop; right = $sourceRight; bottom = $sourceBottom
    }
    region = [ordered]@{ x = $selection.X; y = $selection.Y; width = $selection.Width; height = $selection.Height }
    cropping = $crop
    mode = $modeName
    modeIndex = $modeIndex
    vsrOnlyMode = $vsrOnlyName
    vsrOnlyModeIndex = $vsrOnlyIndex
    profile = $profile.name
    backup = $backup
    restartedMagpie = !$NoRestart
    effectShortcutSent = $effectsStarted
} | ConvertTo-Json -Depth 5

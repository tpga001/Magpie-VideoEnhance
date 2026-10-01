param(
    [ValidateSet('state', 'quit')][string]$Action = 'state',
    [string]$RuntimeDirectory = 'E:\Program Files\Magpie-Experimental-x64'
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $RuntimeDirectory 'Magpie.exe'
$processes = @(Get-Process -Name Magpie -ErrorAction SilentlyContinue | Where-Object Path -eq $exe)
if ($Action -eq 'quit' -and $processes.Count) {
    Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class MagpieSmokeQuit {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern uint RegisterWindowMessage(string name);
    [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls, string name);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr window, uint msg, IntPtr w, IntPtr l);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr window, out uint pid);
}
'@
    $window = [MagpieSmokeQuit]::FindWindow('Magpie_NotifyIcon', $null)
    [uint32]$windowPid = 0
    [void][MagpieSmokeQuit]::GetWindowThreadProcessId($window, [ref]$windowPid)
    if ($window -eq [IntPtr]::Zero -or $windowPid -notin $processes.Id) { throw 'Expected runtime notification window not found' }
    $message = [MagpieSmokeQuit]::RegisterWindowMessage('WM_MAGPIE_QUIT')
    if (![MagpieSmokeQuit]::PostMessage($window, $message, [IntPtr]::Zero, [IntPtr]::Zero)) { throw 'Quit request failed' }
    foreach ($process in $processes) {
        if (!$process.WaitForExit(10000)) { throw 'Graceful quit timed out' }
    }
}
$remaining = @(Get-Process -Name Magpie -ErrorAction SilentlyContinue | Where-Object Path -eq $exe)
@{ running = [bool]$remaining.Count; pids = @($remaining.Id) } | ConvertTo-Json -Compress

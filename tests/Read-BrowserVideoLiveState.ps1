param([ValidateRange(1,45)][int]$Seconds=1,[string]$OutputPath,[long[]]$BrowserHandles=@())
$ErrorActionPreference='Stop'
# Read-only diagnostic: never activates, moves, resizes or closes a window.
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class LiveVideoState {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] public struct MI {
  public int Size; public RECT Monitor,Work; public uint Flags;
  [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)] public string Device;
 }
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c,string n);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr GetProp(IntPtr h,string n);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] public static extern IntPtr MonitorFromWindow(IntPtr h,uint f);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool GetMonitorInfo(IntPtr h,ref MI i);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h,StringBuilder t,int n);
 [DllImport("user32.dll")] public static extern IntPtr GetWindowLongPtrW(IntPtr h,int i);
 public static string Title(IntPtr h) { var t=new StringBuilder(512);GetWindowText(h,t,t.Capacity);return t.ToString(); }
 public static RECT Rect(IntPtr h) { RECT r;GetWindowRect(h,out r);return r; }
 public static MI Monitor(IntPtr h) { var i=new MI{Size=Marshal.SizeOf<MI>()};GetMonitorInfo(MonitorFromWindow(h,2),ref i);return i; }
}
'@
[void][LiveVideoState]::SetThreadDpiAwarenessContext([IntPtr](-4))
$samples=[Collections.Generic.List[object]]::new();$timer=[Diagnostics.Stopwatch]::StartNew()
while($timer.Elapsed.TotalSeconds -lt $Seconds){
 $overlay=[LiveVideoState]::FindWindow('Window_Magpie_967EB565-6F73-4E94-AE53-00CC42592A22',$null)
 $source=if($overlay -ne [IntPtr]::Zero){[LiveVideoState]::GetProp($overlay,'Magpie.SrcHWND')}else{[IntPtr]::Zero}
 $foreground=[LiveVideoState]::GetForegroundWindow()
 $windows=@(foreach($handle in $BrowserHandles){$h=[IntPtr]$handle;$m=[LiveVideoState]::Monitor($h);@{hwnd=$handle;title=[LiveVideoState]::Title($h);visible=[LiveVideoState]::IsWindowVisible($h);bounds=[LiveVideoState]::Rect($h);monitor=$m.Device}})
 $samples.Add(@{time=(Get-Date -Format o);foreground=$foreground.ToInt64();source=$source.ToInt64();sourceTitle=$(if($source -ne [IntPtr]::Zero){[LiveVideoState]::Title($source)}else{''});overlay=$overlay.ToInt64();overlayVisible=($overlay -ne [IntPtr]::Zero -and [LiveVideoState]::IsWindowVisible($overlay));overlayTopmost=($overlay -ne [IntPtr]::Zero -and (([LiveVideoState]::GetWindowLongPtrW($overlay,-20).ToInt64() -band 8) -ne 0));overlayRect=$(if($overlay -ne [IntPtr]::Zero){[LiveVideoState]::Rect($overlay)}else{$null});windows=$windows})
 Start-Sleep -Milliseconds 1000
}
if($OutputPath){$samples | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $OutputPath -Encoding utf8}
$samples | Group-Object foreground,source,overlayVisible | ForEach-Object {@{samples=$_.Count;first=$_.Group[0];lastTime=$_.Group[-1].time}} | ConvertTo-Json -Depth 7

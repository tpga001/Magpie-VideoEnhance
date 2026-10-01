param(
 [ValidateSet('monitors','state','move','capture','observe','burst')][string]$Action='state',
 [string]$Device,
 [string]$OutputPath,
 [int]$Seconds=25,
 [double]$Fraction=0.90,
 [string]$RegionJson,
 [ValidateRange(2,180)][int]$BurstCount=30
)
$ErrorActionPreference='Stop'
Add-Type @'
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class VideoDisplayProbe {
 public delegate bool EnumProc(IntPtr h,IntPtr d);
 public delegate bool MonitorProc(IntPtr h,IntPtr dc,IntPtr rect,IntPtr d);
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left,Top,Right,Bottom; }
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] public struct MI {
  public int Size; public RECT Monitor,Work; public uint Flags;
  [MarshalAs(UnmanagedType.ByValTStr,SizeConst=32)] public string Device;
 }
 [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll")] public static extern bool EnumDisplayMonitors(IntPtr dc,IntPtr clip,MonitorProc cb,IntPtr d);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern bool GetMonitorInfo(IntPtr h,ref MI info);
 [DllImport("user32.dll")] public static extern IntPtr MonitorFromWindow(IntPtr h,uint flags);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb,IntPtr d);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr h,StringBuilder t,int n);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string c,string n);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h,IntPtr after,int x,int y,int w,int ht,uint flags);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr GetProp(IntPtr h,string name);
 [DllImport("user32.dll")] public static extern IntPtr GetWindowLongPtrW(IntPtr h,int index);
 public static string Title(IntPtr h) { var t=new StringBuilder(512);GetWindowText(h,t,t.Capacity);return t.ToString(); }
 public static IntPtr Fixture() { IntPtr found=IntPtr.Zero; EnumWindows((h,d)=>{if(IsWindowVisible(h)&&Title(h).StartsWith("Magpie Decoded 1080p30")){found=h;return false;}return true;},IntPtr.Zero);return found; }
 public static MI Info(IntPtr monitor) { var i=new MI {Size=Marshal.SizeOf<MI>()};if(!GetMonitorInfo(monitor,ref i))throw new Exception("GetMonitorInfo failed");return i; }
 public static MI[] Monitors() { var list=new List<MI>();EnumDisplayMonitors(IntPtr.Zero,IntPtr.Zero,(h,dc,r,d)=>{list.Add(Info(h));return true;},IntPtr.Zero);return list.ToArray(); }
 public static RECT Rect(IntPtr h) { RECT r;if(!GetWindowRect(h,out r))throw new Exception("GetWindowRect failed");return r; }
}
'@
[void][VideoDisplayProbe]::SetThreadDpiAwarenessContext([IntPtr](-4))
if($Action -eq 'monitors') { [VideoDisplayProbe]::Monitors() | ConvertTo-Json -Depth 4; exit }
$browser=[VideoDisplayProbe]::Fixture()
if($browser -eq [IntPtr]::Zero){throw 'Dedicated video fixture window not found'}
if($Action -eq 'move') {
 $monitor=@([VideoDisplayProbe]::Monitors() | Where-Object Device -eq $Device)
 if($monitor.Count -ne 1){throw 'Requested monitor not found'}
 $work=$monitor[0].Work
 $width=[int](($work.Right-$work.Left)*$Fraction);$height=[int](($work.Bottom-$work.Top)*$Fraction)
 for($i=0;$i -lt 3;$i++){
  if(![VideoDisplayProbe]::SetWindowPos($browser,[IntPtr]::Zero,$work.Left+25,$work.Top+25,$width,$height,0x14)){throw 'Fixture move failed'}
  Start-Sleep -Milliseconds 300
 }
}
function Get-ProbeState {
 $overlay=[VideoDisplayProbe]::FindWindow('Window_Magpie_967EB565-6F73-4E94-AE53-00CC42592A22',$null)
 $bounds=[VideoDisplayProbe]::Rect($browser)
 $info=[VideoDisplayProbe]::Info([VideoDisplayProbe]::MonitorFromWindow($browser,2))
 $props=@{}
 if($overlay -ne [IntPtr]::Zero){foreach($name in @('SrcLeft','SrcTop','SrcRight','SrcBottom','DestLeft','DestTop','DestRight','DestBottom')){$props[$name]=[VideoDisplayProbe]::GetProp($overlay,$name).ToInt64()}}
 @{
  time=(Get-Date -Format o);browser=$browser.ToInt64();foreground=[VideoDisplayProbe]::GetForegroundWindow().ToInt64()
  monitor=$info.Device;bounds=$bounds;overlay=$overlay.ToInt64();overlayRect=$(if($overlay -ne [IntPtr]::Zero){[VideoDisplayProbe]::Rect($overlay)}else{$null})
  overlayVisible=($overlay -ne [IntPtr]::Zero -and [VideoDisplayProbe]::IsWindowVisible($overlay))
  overlayTopmost=($overlay -ne [IntPtr]::Zero -and (([VideoDisplayProbe]::GetWindowLongPtrW($overlay,-20).ToInt64() -band 8) -ne 0))
  props=$props
 }
}
if($Action -eq 'observe') {
 $samples=[Collections.Generic.List[object]]::new();$clock=[Diagnostics.Stopwatch]::StartNew()
 while($clock.Elapsed.TotalSeconds -lt $Seconds){$samples.Add((Get-ProbeState));Start-Sleep -Milliseconds 100}
 $samples | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $OutputPath -Encoding utf8
 @{samples=$samples.Count;seconds=$clock.Elapsed.TotalSeconds;foregroundLoss=@($samples | Where-Object {$_.foreground -ne $_.browser}).Count;overlayMissing=@($samples | Where-Object {!$_.overlayVisible}).Count} | ConvertTo-Json -Compress
 exit
}
$state=Get-ProbeState
if($Action -eq 'burst') {
 if(!$RegionJson -and !$state.overlayVisible){throw 'A visible overlay or explicit region is required for the text sample'}
 Add-Type -AssemblyName System.Drawing
 New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null
 $r=if($RegionJson){$RegionJson | ConvertFrom-Json}else{$state.overlayRect};$w=$r.Right-$r.Left;$h=$r.Bottom-$r.Top
 if($w -le 0 -or $h -le 0 -or $r.Left -lt $state.bounds.Left -or $r.Top -lt $state.bounds.Top -or $r.Right -gt $state.bounds.Right -or $r.Bottom -gt $state.bounds.Bottom){throw 'Text region must stay inside the fixture window'}
 $x=$r.Left+[int]($w*0.04);$y=$r.Top+[int]($h*0.05)
 $bitmap=[Drawing.Bitmap]::new([int]($w*0.65),[int]($h*0.12));$graphics=[Drawing.Graphics]::FromImage($bitmap)
 $samples=[Collections.Generic.List[object]]::new();$clock=[Diagnostics.Stopwatch]::StartNew()
 try {for($i=0;$i -lt $BurstCount;$i++){
  if([VideoDisplayProbe]::GetForegroundWindow() -ne $browser){throw 'Fixture lost foreground during text capture'}
  $graphics.CopyFromScreen($x,$y,0,0,$bitmap.Size)
  $bitmap.Save((Join-Path $OutputPath ('text-{0:00}.png' -f $i)),[Drawing.Imaging.ImageFormat]::Png)
  $samples.Add(@{index=$i;ms=$clock.Elapsed.TotalMilliseconds;foreground=[VideoDisplayProbe]::GetForegroundWindow().ToInt64();overlayVisible=(Get-ProbeState).overlayVisible})
  Start-Sleep -Milliseconds 35
 }}finally{$graphics.Dispose();$bitmap.Dispose()}
 @{region=@{left=$x;top=$y;width=[int]($w*0.65);height=[int]($h*0.12)};samples=$samples} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $OutputPath 'samples.json') -Encoding utf8
}
if($Action -eq 'capture') {
 if(!$OutputPath){throw 'OutputPath required'}
 Add-Type -AssemblyName System.Drawing
 $r=$state.bounds;$bitmap=[Drawing.Bitmap]::new($r.Right-$r.Left,$r.Bottom-$r.Top);$graphics=[Drawing.Graphics]::FromImage($bitmap)
 try{$graphics.CopyFromScreen($r.Left,$r.Top,0,0,$bitmap.Size);$bitmap.Save($OutputPath,[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
}
$state | ConvertTo-Json -Depth 5 -Compress

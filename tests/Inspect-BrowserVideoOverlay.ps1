param([string]$OutputPath, [double]$HoleX=0.5, [double]$HoleY=0.5)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class BrowserOverlayInspect {
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
 [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr value);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string name);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd,out RECT rect);
 [DllImport("user32.dll")] public static extern int GetWindowRgn(IntPtr hwnd,IntPtr region);
 [DllImport("gdi32.dll")] public static extern IntPtr CreateRectRgn(int l,int t,int r,int b);
 [DllImport("gdi32.dll")] public static extern bool PtInRegion(IntPtr r,int x,int y);
 [DllImport("gdi32.dll")] public static extern bool DeleteObject(IntPtr value);
}
'@
[void][BrowserOverlayInspect]::SetProcessDpiAwarenessContext([IntPtr](-4))
$window=[BrowserOverlayInspect]::FindWindow('Window_Magpie_967EB565-6F73-4E94-AE53-00CC42592A22',$null)
if($window -eq [IntPtr]::Zero){throw 'Video overlay window not found'}
$rect=[BrowserOverlayInspect+RECT]::new()
if(![BrowserOverlayInspect]::GetWindowRect($window,[ref]$rect)){throw 'Cannot read video overlay rectangle'}
$width=$rect.R-$rect.L; $height=$rect.B-$rect.T
$region=[BrowserOverlayInspect]::CreateRectRgn(0,0,0,0)
try {
 $kind=[BrowserOverlayInspect]::GetWindowRgn($window,$region)
 $holeCovered=[BrowserOverlayInspect]::PtInRegion($region,[int]($HoleX*$width),[int]($HoleY*$height))
 if($OutputPath){
  $bitmap=[Drawing.Bitmap]::new($width,$height)
  $graphics=[Drawing.Graphics]::FromImage($bitmap)
  try {$graphics.CopyFromScreen($rect.L,$rect.T,0,0,[Drawing.Size]::new($width,$height));$bitmap.Save($OutputPath,[Drawing.Imaging.ImageFormat]::Png)}
  finally {$graphics.Dispose();$bitmap.Dispose()}
 }
 @{width=$width;height=$height;regionKind=$kind;holeCovered=$holeCovered;left=$rect.L;top=$rect.T}|ConvertTo-Json -Compress
}finally{[void][BrowserOverlayInspect]::DeleteObject($region)}

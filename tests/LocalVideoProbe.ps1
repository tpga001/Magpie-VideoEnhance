param([int]$PlayerId=0,[string]$Screenshot,[switch]$Focus)
$ErrorActionPreference='Stop'
Add-Type @'
using System;using System.Text;using System.Runtime.InteropServices;using System.Collections.Generic;
public class LocalVideoProbe {
 public delegate bool CB(IntPtr w,IntPtr p);
 [DllImport("user32")] public static extern bool EnumWindows(CB c,IntPtr p);
 [DllImport("user32")] public static extern bool EnumChildWindows(IntPtr w,CB c,IntPtr p);
 [DllImport("user32")] public static extern uint GetWindowThreadProcessId(IntPtr w,out uint p);
 [DllImport("user32",CharSet=CharSet.Unicode)] public static extern int GetClassName(IntPtr w,StringBuilder b,int n);
 [DllImport("user32",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr w,StringBuilder b,int n);
 [DllImport("user32")] public static extern bool GetWindowRect(IntPtr w,out R r);
 [DllImport("user32")] public static extern bool IsWindowVisible(IntPtr w);
 [DllImport("user32")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32")] public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("kernel32")] public static extern uint GetCurrentThreadId();
 [DllImport("user32")] public static extern bool AttachThreadInput(uint a,uint b,bool attach);
 [DllImport("user32")] public static extern bool BringWindowToTop(IntPtr w);
 [DllImport("user32")] public static extern bool ShowWindow(IntPtr w,int n);
 [DllImport("user32")] public static extern bool SetWindowPos(IntPtr w,IntPtr after,int x,int y,int width,int height,uint flags);
 [DllImport("user32")] public static extern bool PostMessageW(IntPtr w,uint m,IntPtr a,IntPtr b);
 [DllImport("user32",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string name);
 [DllImport("user32",CharSet=CharSet.Unicode)] public static extern IntPtr GetProp(IntPtr w,string name);
 [DllImport("user32")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
 public struct R {public int Left,Top,Right,Bottom;}
 public static object Info(IntPtr w){uint p;GetWindowThreadProcessId(w,out p);var c=new StringBuilder(256);var t=new StringBuilder(512);GetClassName(w,c,256);GetWindowText(w,t,512);R r;GetWindowRect(w,out r);return new {hwnd=w.ToInt64(),pid=p,cls=c.ToString(),title=t.ToString(),rect=r,visible=IsWindowVisible(w),foreground=GetForegroundWindow()==w};}
 public static object[] All(uint pid){var l=new List<object>();EnumWindows((w,p)=>{uint id;GetWindowThreadProcessId(w,out id);if(id==pid){l.Add(Info(w));EnumChildWindows(w,(cw,cp)=>{l.Add(Info(cw));return true;},IntPtr.Zero);}return true;},IntPtr.Zero);return l.ToArray();}
 public static bool FocusWindow(IntPtr w){uint pid;uint fg=GetWindowThreadProcessId(GetForegroundWindow(),out pid),self=GetCurrentThreadId();bool linked=fg!=self && AttachThreadInput(self,fg,true);try{ShowWindow(w,9);BringWindowToTop(w);return SetForegroundWindow(w);}finally{if(linked)AttachThreadInput(self,fg,false);}}
}
'@
[void][LocalVideoProbe]::SetThreadDpiAwarenessContext([IntPtr](-4))
if(!$PlayerId){$PlayerId=(Get-Process PotPlayerMini64 | Select-Object -First 1).Id}
$windows=[LocalVideoProbe]::All($PlayerId)
if($Focus){$hwnd=[IntPtr]($windows | Where-Object cls -eq 'PotPlayer64' | Select-Object -First 1).hwnd;[void][LocalVideoProbe]::FocusWindow($hwnd)}
if($Screenshot){
 Add-Type -AssemblyName System.Drawing
 $r=($windows | Where-Object {$_.visible -and $_.cls -like 'PotPlayer*'} | Select-Object -First 1).rect
 $b=[Drawing.Bitmap]::new($r.Right-$r.Left,$r.Bottom-$r.Top);$g=[Drawing.Graphics]::FromImage($b)
 try{$g.CopyFromScreen($r.Left,$r.Top,0,0,$b.Size);$b.Save($Screenshot)}finally{$g.Dispose();$b.Dispose()}
}
$windows | ConvertTo-Json -Depth 4

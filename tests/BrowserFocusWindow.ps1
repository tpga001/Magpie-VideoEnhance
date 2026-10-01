param(
 [ValidateSet('inspect','focusBrowser','focusApp','pulse','app')][string]$Action='inspect',
 [ValidateRange(1,1000)][int]$PulseMilliseconds=150
)
$ErrorActionPreference='Stop'
if($Action -eq 'app') {
 Add-Type -AssemblyName System.Windows.Forms
 Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ShowFocusFixture {
 [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr hwnd,int command);
}
'@
 $form=[Windows.Forms.Form]::new()
 $form.Text='Magpie Focus Regression App'
 $form.Width=750;$form.Height=500
 $label=[Windows.Forms.Label]::new();$label.Dock='Fill'
 $label.Text='Foreground regression fixture. Background video must not cover this window.'
 $form.Controls.Add($label)
 # The hidden PowerShell host startup flag can hide its first GUI window too.
 # Explicitly show only this disposable test form; keep the console hidden.
 $timer=[Windows.Forms.Timer]::new();$timer.Interval=100
 $timer.Add_Tick({$timer.Stop();[void][ShowFocusFixture]::ShowWindow($form.Handle,4)})
 $timer.Start()
 [Windows.Forms.Application]::Run($form)
 $timer.Dispose();$form.Dispose()
 exit
}
Add-Type @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class BrowserFocusWindow {
 public delegate bool EnumProc(IntPtr hwnd,IntPtr data);
 [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L,T,R,B; }
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc callback,IntPtr data);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hwnd,StringBuilder text,int size);
 [DllImport("user32.dll",CharSet=CharSet.Unicode)] public static extern IntPtr FindWindow(string cls,string title);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern void SwitchToThisWindow(IntPtr hwnd,bool altTab);
 [DllImport("user32.dll")] public static extern void keybd_event(byte key,byte scan,uint flags,UIntPtr extra);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr hwnd);
 [DllImport("user32.dll")] public static extern IntPtr GetWindowLongPtrW(IntPtr hwnd,int index);
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hwnd,out uint pid);
 [DllImport("kernel32.dll")] public static extern uint GetCurrentThreadId();
 [DllImport("user32.dll")] public static extern bool AttachThreadInput(uint a,uint b,bool attach);
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr hwnd,out RECT rect);
 public static string Title(IntPtr hwnd) { var text=new StringBuilder(512);GetWindowText(hwnd,text,text.Capacity);return text.ToString(); }
 public static IntPtr FindFixture(string prefix) {
  IntPtr result=IntPtr.Zero;
  EnumWindows((hwnd,data)=>{if(IsWindowVisible(hwnd)&&Title(hwnd).StartsWith(prefix)){result=hwnd;return false;}return true;},IntPtr.Zero);
  return result;
 }
 public static bool Activate(IntPtr hwnd) {
  uint pid;uint current=GetCurrentThreadId(),foreground=GetWindowThreadProcessId(GetForegroundWindow(),out pid);
  bool attached=current!=foreground&&AttachThreadInput(current,foreground,true);
  try {
   SetForegroundWindow(hwnd);
   if(GetForegroundWindow()!=hwnd) SwitchToThisWindow(hwnd,true);
   if(GetForegroundWindow()!=hwnd) {
    // Match the existing browser smoke fixture's test-only Alt activation.
    // Production enhancement never sends keyboard input or activates windows.
    keybd_event(0x12,0,0,UIntPtr.Zero);keybd_event(0x12,0,2,UIntPtr.Zero);
    SetForegroundWindow(hwnd);
   }
   return GetForegroundWindow()==hwnd;
  }
  finally { if(attached)AttachThreadInput(current,foreground,false); }
 }
 public static bool Focus(IntPtr hwnd) {
  bool activated=Activate(hwnd);System.Threading.Thread.Sleep(100);
  return activated&&GetForegroundWindow()==hwnd;
 }
 public static bool Pulse(IntPtr away,IntPtr back,int milliseconds) {
  if(!Activate(away))return false;
  System.Threading.Thread.Sleep(milliseconds);
  if(!Activate(back))return false;
  System.Threading.Thread.Sleep(100);
  return GetForegroundWindow()==back;
 }
}
'@
if($Action -eq 'pulse') {
 $away=[BrowserFocusWindow]::FindFixture('Magpie Focus Regression App')
 $back=[BrowserFocusWindow]::FindFixture('Magpie Decoded 1080p30')
 if($away -eq [IntPtr]::Zero -or $back -eq [IntPtr]::Zero){throw 'Pulse test window not found'}
 if(![BrowserFocusWindow]::Pulse($away,$back,$PulseMilliseconds)){throw 'OS foreground pulse failed'}
} elseif($Action -like 'focus*') {
 $prefix=if($Action -eq 'focusBrowser'){'Magpie Decoded 1080p30'}else{'Magpie Focus Regression App'}
 $target=[BrowserFocusWindow]::FindFixture($prefix)
 if($target -eq [IntPtr]::Zero){throw "Test window not found: $prefix"}
 if(![BrowserFocusWindow]::Focus($target)){throw "OS foreground activation refused: $prefix"}
}
$foreground=[BrowserFocusWindow]::GetForegroundWindow()
$overlay=[BrowserFocusWindow]::FindWindow('Window_Magpie_967EB565-6F73-4E94-AE53-00CC42592A22',$null)
$toast=[BrowserFocusWindow]::FindWindow('Magpie_Toast',$null)
$toastRect=[BrowserFocusWindow+RECT]::new()
[void][BrowserFocusWindow]::GetWindowRect($toast,[ref]$toastRect)
@{
 foreground=$foreground.ToInt64();foregroundTitle=[BrowserFocusWindow]::Title($foreground)
 overlayVisible=($overlay -ne [IntPtr]::Zero -and [BrowserFocusWindow]::IsWindowVisible($overlay))
 overlayTopmost=($overlay -ne [IntPtr]::Zero -and (([BrowserFocusWindow]::GetWindowLongPtrW($overlay,-20).ToInt64() -band 8) -ne 0))
 toastVisible=($toast -ne [IntPtr]::Zero -and [BrowserFocusWindow]::IsWindowVisible($toast) -and $toastRect.R -gt $toastRect.L -and $toastRect.B -gt $toastRect.T)
 fixtureVisible=([BrowserFocusWindow]::FindFixture('Magpie Focus Regression App') -ne [IntPtr]::Zero)
}|ConvertTo-Json -Compress

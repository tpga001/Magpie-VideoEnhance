using System.Diagnostics;
using System.IO.MemoryMappedFiles;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;

namespace Magpie.LocalVideoHost;

internal sealed class Host : ApplicationContext
{
    private readonly NotifyIcon icon;
    private readonly ToolStripMenuItem toggle = new("本地视频增强：已开启");
    private readonly ToolStripMenuItem statusItem = new("等待 PotPlayer 播放") { Enabled = false };
    private readonly System.Windows.Forms.Timer timer = new() { Interval = 200 };
    private readonly string runtime;
    private readonly string config;
    private bool enabled = true;
    private EventWaitHandle? tapEvent;
    private int tapPid;
    private string session = "";
    private ulong lastSequence;
    private long lastAdvance;
    private string lastGeometry = "";
    private long geometrySince;
    private string lastStatus = "";
    private const string MainClass = "PotPlayer64";

    [STAThread] private static void Main(string[] args)
    {
        using var single = new Mutex(true, "Local\\MagpieLocalVideoHost", out bool first);
        if (!first) return;
        ApplicationConfiguration.Initialize();
        Application.Run(new Host(args));
    }

    private Host(string[] args)
    {
        runtime = args.Length > 1 && args[0] == "--runtime" ? Path.GetFullPath(args[1]) : AppContext.BaseDirectory;
        config = Directory.Exists(Path.Combine(runtime,"config","v4e")) ? Path.Combine(runtime,"config","v4e") :
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Magpie","config","v4e");
        var menu = new ContextMenuStrip();
        menu.Items.Add(toggle); menu.Items.Add(statusItem); menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("退出本地视频增强",null,(_,_)=>ExitThread());
        icon = new NotifyIcon { Icon=SystemIcons.Application, Text="Magpie 本地视频增强", ContextMenuStrip=menu, Visible=true };
        toggle.Click += (_,_)=>{enabled=!enabled;toggle.Text=enabled?"本地视频增强：已开启":"本地视频增强：已关闭";if(enabled)StartMagpie();else StopSession();};
        icon.DoubleClick += (_,_)=>toggle.PerformClick();
        timer.Tick += (_,_)=>Tick();
        StartMagpie();timer.Start();
    }

    private static bool MagpieRunning() => Native.FindWindowW("Magpie_NotifyIcon",null)!=0 ||
        Process.GetProcessesByName("Magpie").Length>0;
    private void StartMagpie()
    {
        // Called only by an explicit launch / switch-on. Polling never restarts a tray exit.
        if(MagpieRunning())return;
        string exe=Path.Combine(runtime,"Magpie.exe");
        if(File.Exists(exe))Process.Start(new ProcessStartInfo(exe,"-t"){WorkingDirectory=runtime,UseShellExecute=true,WindowStyle=ProcessWindowStyle.Hidden});
    }
    private void SetStatus(string value)
    {
        statusItem.Text=value;
        if(lastStatus==value)return;lastStatus=value;
        try{Directory.CreateDirectory(Path.Combine(runtime,"logs"));File.AppendAllText(Path.Combine(runtime,"logs","local-video-host.log"),$"{DateTimeOffset.Now:O} {value}\n");}catch(IOException){}
    }
    private void Tick()
    {
        try
        {
            if(!enabled || !MagpieRunning()){StopSession();SetStatus(enabled?"Magpie 已退出；重新开关以启动":"已关闭");return;}
            nint window=Native.GetAncestor(Native.GetForegroundWindow(),2);
            if(Native.Class(window)!=MainClass || Native.IsIconic(window)){StopSession();SetStatus("等待前台 PotPlayer 播放");return;}
            Native.GetWindowThreadProcessId(window,out uint pid);
            using var process=Process.GetProcessById((int)pid);
            if(!process.ProcessName.Equals("PotPlayerMini64",StringComparison.OrdinalIgnoreCase)){StopSession();return;}
            if(tapPid!=(int)pid)
            {
                StopSession();tapPid=(int)pid;lastSequence=0;lastAdvance=Environment.TickCount64;
                tapEvent=new EventWaitHandle(true,EventResetMode.ManualReset,$"Local\\MagpieLocalVideoEnabled_{pid}");tapEvent.Set();
            }
            using var map=MemoryMappedFile.OpenExisting($"Local\\MagpieLocalVideo_{pid}",MemoryMappedFileRights.Read);
            using var view=map.CreateViewAccessor(0,248,MemoryMappedFileAccess.Read);
            using var guard=Mutex.OpenExisting($"Local\\MagpieLocalVideo_{pid}_Mutex");
            if(!TryLock(guard,0))return;
            uint running,w,h;ulong sequence;long heartbeat,interval;string name;
            try
            {
                if(view.ReadUInt32(0)!=0x31564c4d || view.ReadUInt32(4)!=1 || view.ReadUInt32(8)!=pid)return;
                running=view.ReadUInt32(12);w=view.ReadUInt32(16);h=view.ReadUInt32(20);sequence=view.ReadUInt64(32);
                heartbeat=view.ReadInt64(40);interval=view.ReadInt64(48);
                byte[] bytes=new byte[192];view.ReadArray(56,bytes,0,bytes.Length);name=Encoding.Unicode.GetString(bytes).TrimEnd('\0');
            }
            finally{guard.ReleaseMutex();}
            if(sequence!=lastSequence){lastSequence=sequence;lastAdvance=Environment.TickCount64;}
            if(running==0 || Environment.TickCount64-lastAdvance>800 || Qpc100ns()-heartbeat>10000000)
            {DisableOverlay();SetStatus("已暂停或等待有效视频帧");return;}
            if(w<64 || h<64 || sequence==0){SetStatus("等待 SDR RGB32 原始视频帧");return;}
            if(interval>0 && (interval<300000 || interval>500000))
            {DisableOverlay();SetStatus("当前源帧率不在本版 20–33 FPS 补帧范围");return;}
            var area=Native.VideoRect(window,w,h);
            if(area is null){DisableOverlay();SetStatus("未找到视频画布，保留原始播放");return;}
            var r=area.Value;
            string geometry=$"{window}:{r.Left}:{r.Top}:{r.Right}:{r.Bottom}:{w}:{h}:{name}";
            if(geometry!=lastGeometry){lastGeometry=geometry;geometrySince=Environment.TickCount64;DisableOverlay();return;}
            if(Environment.TickCount64-geometrySince<500)return;
            session=$"potplayer:{pid}:{name[^32..]}";
            Send(new {action="update",sessionId=session,localPlayer=new{hwnd=(ulong)window,pid},
                rect=new{left=r.Left,top=r.Top,width=r.Right-r.Left,height=r.Bottom-r.Top},
                frameSource=new{mapName=name,width=w,height=h}});
            double nominalOutput=interval>0?Math.Min(30,10000000.0/interval)*2:60;
            SetStatus($"已送入增强：{w}×{h} → 当前屏幕，2 倍补帧约 {nominalOutput:0} 帧");
        }
        catch(FileNotFoundException){DisableOverlay();SetStatus("未接入本地视频滤镜；重新打开视频后生效");}
        catch(Exception ex) when(ex is IOException or UnauthorizedAccessException or ArgumentException or InvalidOperationException or AbandonedMutexException)
        {DisableOverlay();SetStatus("等待播放器："+ex.Message);}
    }
    private static long Qpc100ns()=> (long)(Stopwatch.GetTimestamp()*(10000000.0/Stopwatch.Frequency));
    private static bool TryLock(Mutex guard,int timeout)
    {
        try{return guard.WaitOne(timeout);}
        catch(AbandonedMutexException){return true;} // Ownership is granted; the caller must release it.
    }
    private void Send(object request)
    {
        if(!MagpieRunning())return;
        using var guard=new Mutex(false,"Local\\MagpieBrowserVideoAutoRequest");if(!TryLock(guard,50))return;
        try
        {
            Directory.CreateDirectory(config);string path=Path.Combine(config,"browser-video-request.json");
            string tmp=path+".local.tmp";File.WriteAllText(tmp,JsonSerializer.Serialize(request),new UTF8Encoding(false));File.Move(tmp,path,true);
            Native.PostMessageW((nint)0xffff,Native.RegisterWindowMessageW("WM_MAGPIE_BROWSER_VIDEO"),0,0);
        }
        finally{guard.ReleaseMutex();}
    }
    private void DisableOverlay(){if(session.Length==0)return;Send(new{action="disable",sessionId=session});session="";}
    private void StopSession(){DisableOverlay();tapEvent?.Reset();tapEvent?.Dispose();tapEvent=null;tapPid=0;lastGeometry="";}
    protected override void ExitThreadCore(){timer.Stop();StopSession();icon.Visible=false;icon.Dispose();base.ExitThreadCore();}
}

internal static class Native
{
    internal delegate bool EnumProc(nint window,nint data);
    [StructLayout(LayoutKind.Sequential)] internal struct Rect {public int Left,Top,Right,Bottom;}
    [DllImport("user32.dll")] internal static extern nint GetForegroundWindow();
    [DllImport("user32.dll")] internal static extern nint GetAncestor(nint w,uint f);
    [DllImport("user32.dll")] internal static extern bool IsIconic(nint w);
    [DllImport("user32.dll")] internal static extern bool IsWindowVisible(nint w);
    [DllImport("user32.dll")] internal static extern uint GetWindowThreadProcessId(nint w,out uint p);
    [DllImport("user32.dll")] internal static extern bool EnumChildWindows(nint w,EnumProc cb,nint d);
    [DllImport("user32.dll")] internal static extern bool GetWindowRect(nint w,out Rect r);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] internal static extern int GetClassNameW(nint w,StringBuilder s,int n);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] internal static extern nint FindWindowW(string c,string? n);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)] internal static extern uint RegisterWindowMessageW(string n);
    [DllImport("user32.dll")] internal static extern bool PostMessageW(nint w,uint m,nint a,nint b);
    internal static string Class(nint w){var s=new StringBuilder(256);GetClassNameW(w,s,s.Capacity);return s.ToString();}
    internal static Rect? VideoRect(nint window,uint width,uint height)
    {
        // PotPlayer's video canvas is its largest visible AfxWnd child. Keep controls outside.
        Rect? best=null;long area=0;
        EnumChildWindows(window,(child,_)=>{
            if(!IsWindowVisible(child) || Class(child)!="AfxWnd140su" || !GetWindowRect(child,out Rect r))return true;
            long a=(long)(r.Right-r.Left)*(r.Bottom-r.Top);
            if(r.Right-r.Left>=64 && r.Bottom-r.Top>=64 && a>area){area=a;best=r;}return true;
        },0);
        if(best is null)return null;
        Rect canvas=best.Value;double fit=Math.Min((canvas.Right-canvas.Left)/(double)width,(canvas.Bottom-canvas.Top)/(double)height);
        int w=(int)Math.Round(width*fit),h=(int)Math.Round(height*fit);
        int x=canvas.Left+(canvas.Right-canvas.Left-w)/2,y=canvas.Top+(canvas.Bottom-canvas.Top-h)/2;
        return new Rect{Left=x,Top=y,Right=x+w,Bottom=y+h};
    }
}

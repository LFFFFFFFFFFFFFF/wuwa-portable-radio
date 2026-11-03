using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

// Standalone display process. It never opens game memory or captures input.
internal sealed class RadioCard {
    internal string Source="随身电台",Title="等待曲目信息",Artist="",State="",Detail="";
    internal int Pid; internal long Sequence;
    internal static Dictionary<string,string> Read(string path) {
        var result=new Dictionary<string,string>(StringComparer.OrdinalIgnoreCase);
        using(var file=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite|FileShare.Delete))
        using(var reader=new StreamReader(file,Encoding.UTF8,true)) {
            string section="",line;
            while((line=reader.ReadLine())!=null) {
                if(line.Length>4096)continue;
                line=line.Trim();
                if(line.StartsWith("[")&&line.EndsWith("]")){section=line.Substring(1,line.Length-2);continue;}
                int equals=line.IndexOf('=');
                if(equals>0&&!line.StartsWith(";"))result[section+"."+line.Substring(0,equals).Trim()]=line.Substring(equals+1).Trim();
            }
        }
        return result;
    }
    internal static string Get(Dictionary<string,string> values,string key,string fallback) {
        string value;return values.TryGetValue(key,out value)?value:fallback;
    }
    internal static int Number(Dictionary<string,string> values,string key,int fallback,int min,int max) {
        int value;return int.TryParse(Get(values,key,""),out value)?Math.Max(min,Math.Min(max,value)):fallback;
    }
    internal static RadioCard Load(string path,int pid) {
        var values=Read(path);int owner;long revision;
        if(!int.TryParse(Get(values,"Radio.Pid",""),out owner)||owner!=pid||
           !long.TryParse(Get(values,"Radio.Sequence",""),out revision))throw new InvalidDataException("Status owner/revision mismatch");
        return new RadioCard{Pid=owner,Sequence=revision,Source=Get(values,"Radio.Source","随身电台"),
            Title=Get(values,"Radio.Title","等待曲目信息"),Artist=Get(values,"Radio.Artist",""),
            State=Get(values,"Radio.State",""),Detail=Get(values,"Radio.Detail","")};
    }
}

internal sealed class HudWindow:Form {
    internal RadioCard Card=new RadioCard();
    internal string Keys="F7 查看  ·  F8 暂停  ·  F9 音源  ·  F10 下一首";
    internal HudWindow() {
        Text="鸣潮 · 随身电台";FormBorderStyle=FormBorderStyle.None;ShowInTaskbar=false;
        StartPosition=FormStartPosition.Manual;TopMost=true;DoubleBuffered=true;
        AutoScaleMode=AutoScaleMode.None;ClientSize=new Size(448,178);BackColor=Color.FromArgb(16,23,30);Opacity=0.95;
    }
    protected override bool ShowWithoutActivation {get{return true;}}
    protected override CreateParams CreateParams {get{var p=base.CreateParams;p.ExStyle|=0x08000000|0x00000080|0x00000020|0x00080000;return p;}}
    protected override void WndProc(ref Message message) {
        if(message.Msg==0x21){message.Result=new IntPtr(3);return;} // MA_NOACTIVATE
        if(message.Msg==0x84){message.Result=new IntPtr(-1);return;} // HTTRANSPARENT
        base.WndProc(ref message);
    }
    internal static void PaintCard(Graphics g,Rectangle area,RadioCard card,string keys) {
        g.Clear(Color.FromArgb(16,23,30));g.SmoothingMode=SmoothingMode.AntiAlias;
        Color accent=Color.FromArgb(102,232,210);
        using(var pen=new Pen(Color.FromArgb(49,65,74)))g.DrawRectangle(pen,0,0,area.Width-1,area.Height-1);
        using(var brush=new SolidBrush(accent))g.FillRectangle(brush,0,0,4,area.Height);
        using(var small=new Font("Microsoft YaHei UI",9,FontStyle.Regular))
        using(var title=new Font("Microsoft YaHei UI",15,FontStyle.Bold)) {
            Draw(g,"随身电台  /  "+card.Source,small,accent,new Rectangle(20,13,260,22));
            Draw(g,card.State,small,Color.FromArgb(197,210,218),new Rectangle(area.Width-164,13,144,22),true);
            Draw(g,card.Title,title,Color.White,new Rectangle(20,43,area.Width-40,33));
            Draw(g,card.Artist,small,Color.FromArgb(181,198,208),new Rectangle(21,83,area.Width-42,23));
            Draw(g,card.Detail,small,Color.FromArgb(245,206,128),new Rectangle(21,112,area.Width-42,23));
            Draw(g,keys,small,Color.FromArgb(119,143,158),new Rectangle(21,146,area.Width-42,22));
        }
    }
    private static void Draw(Graphics g,string text,Font font,Color color,Rectangle box,bool right=false) {
        TextRenderer.DrawText(g,text,font,box,color,TextFormatFlags.NoPrefix|TextFormatFlags.EndEllipsis|
            TextFormatFlags.SingleLine|TextFormatFlags.VerticalCenter|(right?TextFormatFlags.Right:TextFormatFlags.Left));
    }
    protected override void OnPaint(PaintEventArgs e){PaintCard(e.Graphics,ClientRectangle,Card,Keys);}
}

internal sealed class HudContext:ApplicationContext {
    [StructLayout(LayoutKind.Sequential)]private struct Rect {internal int Left,Top,Right,Bottom;}
    [DllImport("user32.dll")]private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")]private static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
    [DllImport("user32.dll")]private static extern bool GetWindowRect(IntPtr window,out Rect rect);
    [DllImport("user32.dll")]private static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll")]private static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int cx,int cy,uint flags);
    private readonly HudWindow window=new HudWindow();
    private readonly Timer timer=new Timer();
    private readonly Process parent;
    private readonly string path;
    private readonly int pid,seconds,margin;
    private readonly bool always;
    private DateTime until=DateTime.MinValue;
    private DateTime started=DateTime.UtcNow;
    private long revision=-1;
    private int missing;
    internal HudContext(int gamePid,string status,string config) {
        pid=gamePid;path=status;parent=Process.GetProcessById(pid);
        // Retain an actual process handle so PID reuse cannot attach to another app.
        var retained=parent.Handle;
        var settings=new Dictionary<string,string>();
        if(File.Exists(config))settings=RadioCard.Read(config);
        seconds=RadioCard.Number(settings,"HUD.ShowSeconds",6,2,60);
        margin=RadioCard.Number(settings,"HUD.Margin",24,0,300);
        always=RadioCard.Number(settings,"HUD.AlwaysVisible",0,0,1)!=0;
        window.Keys=RadioCard.Get(settings,"Keys.Info","F7")+" 查看  ·  "+RadioCard.Get(settings,"Keys.Pause","F8")+
            " 暂停  ·  "+RadioCard.Get(settings,"Keys.Source","F9")+" 音源  ·  "+RadioCard.Get(settings,"Keys.Next","F10")+" 下一首";
        timer.Interval=200;timer.Tick+=Tick;timer.Start();
    }
    private void Tick(object sender,EventArgs args) {
        if(parent.HasExited){ExitThread();return;}
        try {
            RadioCard card=RadioCard.Load(path,pid);missing=0;
            if(card.Sequence!=revision){revision=card.Sequence;window.Card=card;window.Invalidate();until=DateTime.UtcNow.AddSeconds(seconds);}
        }catch(IOException){missing++;}catch(UnauthorizedAccessException){missing++;}
        if(missing>15&&(revision>=0||DateTime.UtcNow-started>TimeSpan.FromSeconds(15))){ExitThread();return;}
        uint foregroundPid;IntPtr foreground=GetForegroundWindow();GetWindowThreadProcessId(foreground,out foregroundPid);
        if(revision<0||foregroundPid!=(uint)pid||IsIconic(foreground)||(!always&&DateTime.UtcNow>until)) {if(window.Visible)window.Hide();return;}
        Rect bounds;if(!GetWindowRect(foreground,out bounds))return;
        Rectangle work=Screen.FromHandle(foreground).WorkingArea;
        int x=Math.Max(work.Left,Math.Min(bounds.Right,work.Right)-window.Width-margin);
        int y=Math.Max(work.Top,bounds.Top)+margin;
        SetWindowPos(window.Handle,new IntPtr(-1),x,y,0,0,0x0001|0x0010);
        if(!window.Visible)window.Show();
    }
    protected override void ExitThreadCore(){timer.Stop();timer.Dispose();window.Dispose();parent.Dispose();base.ExitThreadCore();}
}

internal static class HudProgram {
    internal static void Render(string path) {
        var card=new RadioCard{Source="网易云",Title="たぶん",Artist="YOASOBI",State="播放中",Detail="已导入列表  1 / 12"};
        using(var bitmap=new Bitmap(448,178))using(var g=Graphics.FromImage(bitmap)){
            HudWindow.PaintCard(g,new Rectangle(0,0,448,178),card,"F7 查看  ·  F8 暂停  ·  F9 音源  ·  F10 下一首");
            bitmap.Save(path,System.Drawing.Imaging.ImageFormat.Png);
        }
    }
    internal static int SelfTest() {
        string file=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"hud-test-"+Guid.NewGuid().ToString("N")+".ini");
        try {
            File.WriteAllText(file,"[Radio]\r\nPid=123\r\nSequence=42\r\nSource=网易云\r\nTitle=たぶん = [Live] & 你好\r\nArtist=YOASOBI\r\nState=切歌待确认\r\nDetail=正在切到：另一首\r\n",Encoding.Unicode);
            var card=RadioCard.Load(file,123);
            if(card.Title!="たぶん = [Live] & 你好"||card.Sequence!=42||card.State!="切歌待确认"||card.Artist!="YOASOBI")throw new Exception("Unicode/status fields");
            bool rejected=false;try{RadioCard.Load(file,124);}catch(InvalidDataException){rejected=true;}
            if(!rejected)throw new Exception("Wrong PID accepted");
            File.WriteAllText(file,"[HUD]\nAlwaysVisible=1\nShowSeconds=9\n",new UTF8Encoding(false));
            var settings=RadioCard.Read(file);
            if(RadioCard.Number(settings,"HUD.AlwaysVisible",0,0,1)!=1||RadioCard.Number(settings,"HUD.ShowSeconds",6,2,60)!=9)throw new Exception("BOM-free configuration");
            using(var bitmap=new Bitmap(448,178))using(var g=Graphics.FromImage(bitmap)){card.Title=new string('歌',400);HudWindow.PaintCard(g,new Rectangle(0,0,448,178),card,"F7 查看");}
            Console.WriteLine("PASS: HUD UTF-16 contract, BOM-free config, Unicode title/artist, PID guard, long-title rendering");return 0;
        }catch(Exception e){Console.Error.WriteLine(e);return 1;}finally{if(File.Exists(file))File.Delete(file);}
    }
    [STAThread]private static int Main(string[] args) {
        if(args.Length==1&&args[0]=="--self-test")return SelfTest();
        if(args.Length==2&&args[0]=="--render-preview"){Render(Path.GetFullPath(args[1]));return 0;}
        int pid;
        if(args.Length!=6||args[0]!="--pid"||!int.TryParse(args[1],out pid)||pid<=0||args[2]!="--status"||args[4]!="--config")return 2;
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        try{Application.Run(new HudContext(pid,Path.GetFullPath(args[3]),Path.GetFullPath(args[5])));return 0;}
        catch(ArgumentException){return 3;}catch(InvalidOperationException){return 3;}catch(IOException){return 3;}catch(UnauthorizedAccessException){return 3;}
    }
}

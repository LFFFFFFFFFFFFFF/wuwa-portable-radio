using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Windows.Forms;

// Standalone UI process. No game-memory access or graphics injection.
internal sealed class RadioCard {
    internal string Source="随身电台",Title="等待曲目信息",Artist="",State="",Detail="";
    internal int Pid,ControlThread; internal long Sequence; internal bool ViewOpen,Riding,BgmEnabled,BgmAvailable,BgmActive;
    internal long MusicId,PositionMs,DurationMs,SampleTick;internal string LocalPath="";
    internal bool Timeline,Playing;internal int RatePermille=1000;
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
    internal static long Long(Dictionary<string,string> values,string key){long value;return long.TryParse(Get(values,key,""),out value)?Math.Max(0,value):0;}
    internal static RadioCard Load(string path,int pid) {
        var values=Read(path);int owner;long revision;
        if(!int.TryParse(Get(values,"Radio.Pid",""),out owner)||owner!=pid||
           !long.TryParse(Get(values,"Radio.Sequence",""),out revision))throw new InvalidDataException("Status owner/revision mismatch");
        return new RadioCard{Pid=owner,Sequence=revision,ControlThread=Number(values,"Radio.ControlThread",0,0,int.MaxValue),ViewOpen=Get(values,"Radio.ViewOpen","0")=="1",Riding=Get(values,"Radio.Riding","0")=="1",Source=Get(values,"Radio.Source","随身电台"),
            Title=Get(values,"Radio.Title","等待曲目信息"),Artist=Get(values,"Radio.Artist",""),
            State=Get(values,"Radio.State",""),Detail=Get(values,"Radio.Detail",""),BgmEnabled=Get(values,"Radio.BgmEnabled","1")=="1",BgmAvailable=Get(values,"Radio.BgmAvailable","0")=="1",BgmActive=Get(values,"Radio.BgmActive","0")=="1",
            MusicId=Long(values,"Radio.MusicId"),LocalPath=Get(values,"Radio.LocalPath",""),PositionMs=Long(values,"Radio.PositionMs"),DurationMs=Long(values,"Radio.DurationMs"),SampleTick=Long(values,"Radio.SampleTick"),Timeline=Get(values,"Radio.Timeline","0")=="1",Playing=Get(values,"Radio.Playing","0")=="1",RatePermille=Number(values,"Radio.RatePermille",1000,250,4000)};
    }
}

internal sealed class HudWindow:Form {
    internal RadioCard Card=new RadioCard();internal readonly bool Addon;
    internal Action<int> Command;internal Action AddSongs,ShowSourceMenu;
    internal HudWindow(bool addon){Addon=addon;Text=addon?"随身电台 · 添加歌曲入口":"随身电台 · 播放条";
        FormBorderStyle=FormBorderStyle.None;ShowInTaskbar=false;StartPosition=FormStartPosition.Manual;TopMost=true;DoubleBuffered=true;AutoScaleMode=AutoScaleMode.None;
        ClientSize=addon?new Size(340,46):new Size(580,76);BackColor=Color.Magenta;TransparencyKey=Color.Magenta;Opacity=0.96;Cursor=Cursors.Hand;
        var tip=new ToolTip();tip.SetToolTip(this,addon?"添加歌曲或切换音源":"- 上一首 / = 下一首 / · 官方电台界面 / 右键切换音源");FormClosed+=delegate{tip.Dispose();};
    }
    protected override bool ShowWithoutActivation {get{return true;}}
    protected override CreateParams CreateParams {get{var p=base.CreateParams;p.ExStyle|=0x08000000|0x80|0x80000;return p;}}
    protected override void WndProc(ref Message message){if(message.Msg==0x21){message.Result=new IntPtr(3);return;}base.WndProc(ref message);}
    internal static void PaintCard(Graphics g,Rectangle area,RadioCard card,bool addon){
        g.SmoothingMode=SmoothingMode.AntiAlias;
        if(addon){
            using(var font=new Font("Microsoft YaHei UI",16,FontStyle.Bold)){
                PaintPill(g,new RectangleF(1,1,211,42),Color.FromArgb(17,25,39),Color.FromArgb(165,176,191));
                PaintPill(g,new RectangleF(224,1,113,42),Color.FromArgb(17,25,39),Color.FromArgb(165,176,191));
                RadioTheme.Label(g,"＋ 添加歌曲",font,new Rectangle(6,3,202,37),RadioTheme.Text,true);
                RadioTheme.Label(g,"音源 ▾",font,new Rectangle(226,3,108,37),RadioTheme.Text,true);
            }return;
        }
        PaintPill(g,new RectangleF(1,1,area.Width-3,area.Height-3),Color.FromArgb(10,32,47),RadioTheme.Cyan);
        using(var edge=new Pen(Color.FromArgb(40,106,126),3))g.DrawArc(edge,7,9,53,53,25,285);
        using(var fill=new SolidBrush(RadioTheme.Cyan))g.FillEllipse(fill,17,19,33,33);
        using(var fill=new SolidBrush(RadioTheme.Ink))g.FillEllipse(fill,25,27,17,17);
        using(var dot=new SolidBrush(RadioTheme.Cyan))g.FillEllipse(dot,31,33,5,5);
        using(var font=new Font("Microsoft YaHei UI",14,FontStyle.Bold))RadioTheme.Label(g,card.Title,font,new Rectangle(66,12,325,33),Color.White);
        using(var wave=new SolidBrush(Color.FromArgb(42,150,172)))for(int i=0;i<59;++i){int h=3+(i*7%13);g.FillRectangle(wave,68+i*5,59-h,2,h);}
        PaintTransportIcon(g,421,36,2,false);PaintTransportIcon(g,481,36,1,card.State=="已暂停"||card.State=="未播放");PaintTransportIcon(g,541,36,3,false);
    }
    private static void PaintPill(Graphics g,RectangleF box,Color fill,Color line){using(var shape=RadioTheme.Round(box,box.Height/2-1))using(var brush=new SolidBrush(fill))using(var pen=new Pen(line,1.4f)){g.FillPath(brush,shape);g.DrawPath(pen,shape);}}
    private static void PaintTransportIcon(Graphics g,int x,int y,int action,bool play){
        using(var glow=new Pen(Color.FromArgb(34,82,101),5))g.DrawEllipse(glow,x-20,y-20,40,40);
        using(var edge=new Pen(RadioTheme.Cyan,1.4f))g.DrawEllipse(edge,x-19,y-19,38,38);
        using(var b=new SolidBrush(Color.White)){
            if(action==1&&!play){g.FillRectangle(b,x-6,y-9,4,18);g.FillRectangle(b,x+2,y-9,4,18);}
            else if(action==1)g.FillPolygon(b,new[]{new Point(x-5,y-10),new Point(x+9,y),new Point(x-5,y+10)});
            else if(action==2){g.FillRectangle(b,x-8,y-8,3,16);g.FillPolygon(b,new[]{new Point(x+7,y-8),new Point(x-3,y),new Point(x+7,y+8)});}
            else{g.FillRectangle(b,x+6,y-8,3,16);g.FillPolygon(b,new[]{new Point(x-6,y-8),new Point(x+4,y),new Point(x-6,y+8)});}
        }
    }
    protected override void OnPaint(PaintEventArgs e){e.Graphics.Clear(Color.Magenta);PaintCard(e.Graphics,ClientRectangle,Card,Addon);}
    protected override void OnMouseUp(MouseEventArgs e){base.OnMouseUp(e);if(e.Button==MouseButtons.Right){if(ShowSourceMenu!=null)ShowSourceMenu();return;}
        if(e.Button!=MouseButtons.Left)return;
        if(Addon){if(e.X<218){if(AddSongs!=null)AddSongs();}else if(ShowSourceMenu!=null)ShowSourceMenu();}
        else if(Command!=null)Command(e.X>=514?3:e.X>=454?1:e.X>=394?2:4);
    }
}
internal sealed class RadioMenuColors:ProfessionalColorTable {
    public override Color ToolStripDropDownBackground {get{return RadioTheme.Ink;}}
    public override Color ImageMarginGradientBegin {get{return RadioTheme.Ink;}}
    public override Color ImageMarginGradientMiddle {get{return RadioTheme.Ink;}}
    public override Color ImageMarginGradientEnd {get{return RadioTheme.Ink;}}
    public override Color MenuItemSelected {get{return RadioTheme.Panel;}}
    public override Color MenuItemBorder {get{return RadioTheme.Gold;}}
    public override Color MenuBorder {get{return Color.FromArgb(109,126,145);}}
}
internal sealed class HudContext:ApplicationContext {
    [StructLayout(LayoutKind.Sequential)]private struct Rect {internal int Left,Top,Right,Bottom;}
    [DllImport("user32.dll")]private static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")]private static extern uint GetWindowThreadProcessId(IntPtr window,out uint pid);
    [DllImport("user32.dll")]private static extern bool GetClientRect(IntPtr window,out Rect rect);
    [DllImport("user32.dll")]private static extern bool ClientToScreen(IntPtr window,ref Point point);
    [DllImport("user32.dll")]private static extern bool IsIconic(IntPtr window);
    [DllImport("user32.dll")]private static extern bool SetWindowPos(IntPtr window,IntPtr after,int x,int y,int cx,int cy,uint flags);
    [DllImport("user32.dll",CharSet=CharSet.Unicode)]private static extern uint RegisterWindowMessage(string name);
    [DllImport("user32.dll")]private static extern bool PostThreadMessage(uint thread,uint message,IntPtr w,IntPtr l);
    [DllImport("kernel32.dll")]private static extern ulong GetTickCount64();
    [DllImport("kernel32.dll",CharSet=CharSet.Unicode)]private static extern bool WritePrivateProfileString(string section,string key,string value,string file);
    private readonly HudWindow mini=new HudWindow(false),addon=new HudWindow(true);
    private readonly Timer timer=new Timer();private readonly Process parent;
    private readonly ContextMenuStrip menu=new ContextMenuStrip();private ImportWindow importer;
    private readonly LyricsWindow subtitles=new LyricsWindow();private readonly LyricsController lyrics;
    private readonly string configPath;private bool lyricsEnabled,translationEnabled;private int lyricsOffset;
    private readonly string path,library;private readonly int pid,seconds;private readonly bool always;
    private readonly uint commandMessage=RegisterWindowMessage("PortableRadio.UI.Commands.v2.4");
    private DateTime until=DateTime.MinValue,started=DateTime.UtcNow;private long revision=-1;private int missing;
    internal HudContext(int gamePid,string status,string config){
        pid=gamePid;path=status;configPath=config;lyrics=new LyricsController(config);library=Path.Combine(Path.GetDirectoryName(config),"PortableRadioNetEase.ini");parent=Process.GetProcessById(pid);var retained=parent.Handle;
        var settings=File.Exists(config)?RadioCard.Read(config):new Dictionary<string,string>();
        seconds=RadioCard.Number(settings,"HUD.ShowSeconds",6,2,60);always=RadioCard.Number(settings,"HUD.AlwaysVisible",1,0,1)!=0;
        lyricsEnabled=RadioCard.Number(settings,"Lyrics.Enabled",1,0,1)!=0;translationEnabled=RadioCard.Number(settings,"Lyrics.Translation",1,0,1)!=0;lyricsOffset=RadioCard.Number(settings,"Lyrics.OffsetMs",0,-10000,10000);
        mini.Command=Send;mini.ShowSourceMenu=ShowMenu;addon.ShowSourceMenu=ShowMenu;addon.AddSongs=OpenImporter;
        menu.Font=new Font("Microsoft YaHei UI",10);menu.ForeColor=RadioTheme.Text;menu.Renderer=new ToolStripProfessionalRenderer(new RadioMenuColors());
        menu.Items.Add("游戏内置电台",null,delegate{Send(7);});menu.Items.Add("网易云音乐",null,delegate{Send(6);});menu.Items.Add("本地音乐",null,delegate{Send(8);});menu.Items.Add(new ToolStripSeparator());
        menu.Items.Add("＋ 添加歌曲",null,delegate{OpenImporter();});menu.Items.Add("打开官方电台界面  ·",null,delegate{Send(4);});
        menu.Items.Add(new ToolStripSeparator());var bgm=new ToolStripMenuItem("播放时屏蔽游戏背景音乐");
        bgm.ToolTipText="控制网易云与本地音乐的背景避让；内置电台沿用官方规则。暂停或停止外部播放后自动恢复。";
        bgm.Click+=delegate{Send(9);};menu.Items.Add(bgm);
        menu.Opening+=delegate{bgm.Checked=mini.Card.BgmEnabled;bgm.Enabled=mini.Card.BgmAvailable;bgm.Text=mini.Card.BgmAvailable?"播放时屏蔽游戏背景音乐":"背景音乐屏蔽（接口未就绪）";};
        menu.Items.Add(new ToolStripSeparator());var captions=new ToolStripMenuItem("歌曲字幕");menu.Items.Add(captions);
        captions.Click+=delegate{lyricsEnabled=!lyricsEnabled;SaveLyrics("Enabled",lyricsEnabled?"1":"0");if(!lyricsEnabled){subtitles.Hide();lyrics.Select(new RadioCard());}};
        var options=new ToolStripMenuItem("字幕设置");menu.Items.Add(options);var translation=new ToolStripMenuItem("显示翻译");options.DropDownItems.Add(translation);
        translation.Click+=delegate{translationEnabled=!translationEnabled;SaveLyrics("Translation",translationEnabled?"1":"0");};
        var timing=new ToolStripMenuItem();timing.Enabled=false;options.DropDownItems.Add(timing);
        options.DropDownItems.Add("字幕提前 0.25 秒",null,delegate{SetLyricsOffset(lyricsOffset+250);});options.DropDownItems.Add("字幕延后 0.25 秒",null,delegate{SetLyricsOffset(lyricsOffset-250);});options.DropDownItems.Add("重置时间偏移",null,delegate{SetLyricsOffset(0);});
        options.DropDownItems.Add(new ToolStripSeparator());options.DropDownItems.Add("重新读取当前歌词",null,delegate{lyrics.Select(mini.Card,true);});
        options.DropDownItems.Add("导入当前歌曲的 LRC…",null,delegate{try{using(var dialog=new OpenFileDialog{Title="导入当前歌曲字幕",Filter="LRC 歌词 (*.lrc)|*.lrc",CheckFileExists=true})if(dialog.ShowDialog()==DialogResult.OK)lyrics.Import(mini.Card,dialog.FileName);}catch(Exception ex){MessageBox.Show(ex.Message,"歌曲字幕");}});
        var lyricStatus=new ToolStripMenuItem();lyricStatus.Enabled=false;options.DropDownItems.Add(lyricStatus);
        menu.Opening+=delegate{captions.Checked=lyricsEnabled;translation.Checked=translationEnabled;timing.Text="时间偏移："+lyricsOffset+" ms（正值提前）";lyricStatus.Text=mini.Card.Source=="游戏内置电台"?"内置电台暂未接入歌词进度":lyrics.Notice.Length>0?lyrics.Notice:"歌词已就绪";};
        timer.Interval=150;timer.Tick+=Tick;timer.Start();
    }
    private void Send(int action){if(mini.Card.ControlThread>0)PostThreadMessage((uint)mini.Card.ControlThread,commandMessage,new IntPtr(Process.GetCurrentProcess().Id),new IntPtr(action));}
    private void SaveLyrics(string key,string value){WritePrivateProfileString("Lyrics",key,value,configPath);}
    private void SetLyricsOffset(int value){lyricsOffset=Math.Max(-10000,Math.Min(10000,value));SaveLyrics("OffsetMs",lyricsOffset.ToString(System.Globalization.CultureInfo.InvariantCulture));}
    private void ShowMenu(){menu.Show(Cursor.Position);}
    private void OpenImporter(){if(importer!=null&&!importer.IsDisposed){importer.Activate();return;}importer=new ImportWindow(library);importer.Show();}
    private static void Place(Form window,int x,int y){SetWindowPos(window.Handle,new IntPtr(-1),x,y,0,0,0x0001|0x0010);if(!window.Visible)window.Show();}
    private void Hide(){if(mini.Visible)mini.Hide();if(addon.Visible)addon.Hide();if(subtitles.Visible)subtitles.Hide();}
    private void Tick(object sender,EventArgs args){
        if(parent.HasExited){ExitThread();return;}
        try{var card=RadioCard.Load(path,pid);missing=0;if(card.Sequence!=revision){bool changed=revision<0||card.Title!=mini.Card.Title||card.State!=mini.Card.State||card.Source!=mini.Card.Source||card.Detail!=mini.Card.Detail;revision=card.Sequence;mini.Card=addon.Card=card;mini.Invalidate();addon.Invalidate();if(changed)until=DateTime.UtcNow.AddSeconds(seconds);}}
        catch(IOException){missing++;}catch(UnauthorizedAccessException){missing++;}
        if(missing>15&&(revision>=0||DateTime.UtcNow-started>TimeSpan.FromSeconds(15))){ExitThread();return;}
        uint foregroundPid;IntPtr foreground=GetForegroundWindow();GetWindowThreadProcessId(foreground,out foregroundPid);
        if(foregroundPid==(uint)Process.GetCurrentProcess().Id){if(importer!=null&&importer.Visible)Hide();return;}
        if(revision<0||foregroundPid!=(uint)pid||IsIconic(foreground)){Hide();return;}
        Rect bounds;Point origin=new Point(0,0);if(!GetClientRect(foreground,out bounds)||!ClientToScreen(foreground,ref origin))return;
        int width=bounds.Right,height=bounds.Bottom;
        if(lyricsEnabled&&(mini.Card.Source=="网易云"||mini.Card.Source=="本地音乐")){
            lyrics.Select(mini.Card);long position=LyricClock.Position(mini.Card,(long)GetTickCount64(),lyricsOffset);
            var line=lyrics.Document!=null&&position>=0?lyrics.Document.At(position):Tuple.Create(lyrics.Notice.Length>0?lyrics.Notice:"等待客户端播放进度","");
            subtitles.Line=line.Item1;subtitles.Translation=translationEnabled?line.Item2:"";subtitles.Note="随身电台 · "+mini.Card.Source+(mini.Card.Playing?"":" · 已暂停／停止");subtitles.Invalidate();
            if(subtitles.Line.Length==0&&subtitles.Translation.Length==0){if(subtitles.Visible)subtitles.Hide();}
            else{subtitles.Width=Math.Max(320,Math.Min(960,width-32));Place(subtitles,origin.X+(width-subtitles.Width)/2,origin.Y+(int)(height*0.79));}
        }else{if(subtitles.Visible)subtitles.Hide();lyrics.Select(new RadioCard());}
        if(mini.Card.ViewOpen){if(mini.Visible)mini.Hide();Place(addon,origin.X+(int)(width*0.155),origin.Y+(int)(height*0.045));}
        else{if(addon.Visible)addon.Hide();if((mini.Card.Riding&&mini.Card.Source=="游戏内置电台")||(!always&&DateTime.UtcNow>until)){if(mini.Visible)mini.Hide();return;}
            Place(mini,origin.X+(width-mini.Width)/2,origin.Y+(int)(height*0.04));}
    }
    protected override void ExitThreadCore(){timer.Stop();timer.Dispose();lyrics.Dispose();subtitles.Dispose();if(importer!=null)importer.Dispose();menu.Dispose();mini.Dispose();addon.Dispose();parent.Dispose();base.ExitThreadCore();}
}
internal static class HudProgram {
    internal static void Render(string path){
        var card=new RadioCard{Source="游戏内置电台",Title="冰封峡湾的寒夜",State="播放中"};
        using(var bar=new Bitmap(580,76))using(var bg=Graphics.FromImage(bar))
        using(var buttons=new Bitmap(340,46))using(var ag=Graphics.FromImage(buttons))
        using(var bitmap=new Bitmap(600,150))using(var g=Graphics.FromImage(bitmap)){
            bg.Clear(RadioTheme.Ink);ag.Clear(RadioTheme.Ink);g.Clear(RadioTheme.Ink);
            HudWindow.PaintCard(bg,new Rectangle(0,0,580,76),card,false);HudWindow.PaintCard(ag,new Rectangle(0,0,340,46),card,true);
            g.DrawImageUnscaled(bar,10,4);g.DrawImageUnscaled(buttons,12,94);bitmap.Save(path,System.Drawing.Imaging.ImageFormat.Png);
        }
    }
    internal static int SelfTest(){string file=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"hud-test-"+Guid.NewGuid().ToString("N")+".ini");try{
        File.WriteAllText(file,"[Radio]\r\nPid=123\r\nSequence=42\r\nControlThread=456\r\nViewOpen=1\r\nRiding=0\r\nBgmEnabled=1\r\nBgmAvailable=1\r\nBgmActive=0\r\nSource=网易云\r\nTitle=たぶん = [Live] & 你好\r\nArtist=YOASOBI\r\nState=切歌待确认\r\nMusicId=1459343252\r\nPositionMs=1000\r\nDurationMs=10000\r\nSampleTick=5000\r\nTimeline=1\r\nPlaying=0\r\nRatePermille=2000\r\n",Encoding.Unicode);
        var card=RadioCard.Load(file,123);if(card.Title!="たぶん = [Live] & 你好"||!card.ViewOpen||card.Riding||card.ControlThread!=456)throw new Exception("HUD contract");
        if(!card.BgmEnabled||!card.BgmAvailable||card.BgmActive)throw new Exception("BGM state contract");
        if(card.MusicId!=1459343252||card.PositionMs!=1000||card.DurationMs!=10000||card.SampleTick!=5000||!card.Timeline||card.Playing||card.RatePermille!=2000)throw new Exception("Subtitle timeline contract");
        bool rejected=false;try{RadioCard.Load(file,124);}catch(InvalidDataException){rejected=true;}if(!rejected)throw new Exception("Wrong PID");
        using(var bitmap=new Bitmap(580,76))using(var g=Graphics.FromImage(bitmap)){card.Title=new string('歌',400);HudWindow.PaintCard(g,new Rectangle(0,0,580,76),card,false);}
        Console.WriteLine("PASS: HUD Unicode/PID/visibility/worker contract and long-title rendering");return 0;
    }catch(Exception e){Console.Error.WriteLine(e);return 1;}finally{if(File.Exists(file))File.Delete(file);}}
    [STAThread]private static int Main(string[] args){
        if(args.Length==1&&args[0]=="--self-test")return SelfTest();
        if(args.Length==1&&args[0]=="--lyrics-self-test")return LyricsTests.Run();
        if(args.Length==2&&args[0]=="--lyrics-probe")return LyricsTests.Probe(args[1]);
        if(args.Length==2&&args[0]=="--render-lyrics-preview"){LyricsTests.Render(Path.GetFullPath(args[1]));return 0;}
        if(args.Length==2&&args[0]=="--render-preview"){Render(Path.GetFullPath(args[1]));return 0;}
        int pid;if(args.Length!=6||args[0]!="--pid"||!int.TryParse(args[1],out pid)||pid<=0||args[2]!="--status"||args[4]!="--config")return 2;
        System.Net.ServicePointManager.SecurityProtocol=System.Net.SecurityProtocolType.Tls12;
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        try{Application.Run(new HudContext(pid,Path.GetFullPath(args[3]),Path.GetFullPath(args[5])));return 0;}
        catch(ArgumentException){return 3;}catch(InvalidOperationException){return 3;}catch(IOException){return 3;}catch(UnauthorizedAccessException){return 3;}
    }
}

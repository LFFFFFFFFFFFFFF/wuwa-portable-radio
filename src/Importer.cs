using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using System.Drawing;

public sealed class Track { public long Id; public string Name,Artist; public long Duration; }
public sealed class ImportData { public string Title;public List<Track> Tracks=new List<Track>(); }
static class NetEase {
    public static Tuple<string,long> Parse(string input) {
        var match=Regex.Match(input,@"https?://[^\s<>\[\]()]+",RegexOptions.IgnoreCase);
        Uri uri;if(!match.Success||!Uri.TryCreate(match.Value,UriKind.Absolute,out uri))throw new Exception("请粘贴网易云歌曲或歌单的完整分享链接。");
        string host=uri.Host.ToLowerInvariant();
        if(host!="music.163.com"&&!host.EndsWith(".music.163.com"))throw new Exception("目前支持 music.163.com 的歌曲和歌单链接。");
        string path=uri.AbsolutePath+uri.Fragment;
        string type=path.IndexOf("playlist",StringComparison.OrdinalIgnoreCase)>=0?"playlist":path.IndexOf("song",StringComparison.OrdinalIgnoreCase)>=0?"song":null;
        var id=Regex.Match(uri.Query+uri.Fragment,@"(?:[?&])id=(\d+)");long number;
        if(type==null||!id.Success||!long.TryParse(id.Groups[1].Value,out number)||number<=0)throw new Exception("链接中没有有效歌曲或歌单 ID。");
        return Tuple.Create(type,number);
    }
    static Dictionary<string,object> Obj(object value){return value as Dictionary<string,object>??new Dictionary<string,object>();}
    static object Get(Dictionary<string,object> d,string key){object v;return d.TryGetValue(key,out v)?v:null;}
    static long Num(object v){try{return Convert.ToInt64(v);}catch{return 0;}}
    static Track Read(object value) {
        var s=Obj(value);var artists=(Get(s,"artists")??Get(s,"ar")) as object[]??new object[0];
        return new Track{Id=Num(Get(s,"id")),Name=Convert.ToString(Get(s,"name")),Artist=string.Join(" / ",artists.Select(a=>Convert.ToString(Get(Obj(a),"name")))),Duration=Num(Get(s,"duration")??Get(s,"dt"))};
    }
    public static async Task<ImportData> Fetch(string text) {
        var reference=Parse(text);string id=reference.Item2.ToString();
        string url=reference.Item1=="song"?"https://music.163.com/api/song/detail/?id="+id+"&ids=%5B"+id+"%5D":"https://music.163.com/api/playlist/detail?id="+id;
        using(var handler=new HttpClientHandler{AutomaticDecompression=DecompressionMethods.GZip|DecompressionMethods.Deflate})
        using(var client=new HttpClient(handler)) {
            client.Timeout=TimeSpan.FromSeconds(25);client.DefaultRequestHeaders.TryAddWithoutValidation("User-Agent","Mozilla/5.0");client.DefaultRequestHeaders.Referrer=new Uri("https://music.163.com/");
            string json=await client.GetStringAsync(url);if(json.Length>16000000)throw new Exception("歌单数据过大，请拆分后导入。");
            var d=Obj(new JavaScriptSerializer{MaxJsonLength=16000000}.DeserializeObject(json));
            if(Num(Get(d,"code"))!=200)throw new Exception("网易云没有返回可读取的歌曲/歌单信息。");
            var result=new ImportData();
            if(reference.Item1=="song") {result.Tracks.AddRange(((Get(d,"songs") as object[])??new object[0]).Select(Read));result.Title=result.Tracks.Count>0?result.Tracks[0].Name:"歌曲";}
            else {var list=Obj(Get(d,"result")??Get(d,"playlist"));result.Title=Convert.ToString(Get(list,"name"));result.Tracks.AddRange(((Get(list,"tracks") as object[])??new object[0]).Select(Read));}
            result.Tracks=result.Tracks.Where(t=>t.Id>0).GroupBy(t=>t.Id).Select(g=>g.First()).Take(2000).ToList();
            if(result.Tracks.Count==0)throw new Exception("没有读到曲目；请使用可以公开查看的歌单链接。");return result;
        }
    }
    static string Clean(string value){return (value??"").Replace("\r"," ").Replace("\n"," ");}
    public static ImportData Load(string path) {
        var data=new ImportData{Title="我的随身电台"};if(!File.Exists(path))return data;
        Track track=null;
        foreach(string raw in File.ReadAllLines(path,Encoding.Unicode)){
            string line=raw.Trim();
            if(line.StartsWith("[Track")&&line.EndsWith("]")){track=new Track();data.Tracks.Add(track);continue;}
            if(line.StartsWith("[")){track=null;continue;}
            int equals=line.IndexOf('=');if(equals<0)continue;string key=line.Substring(0,equals),value=line.Substring(equals+1);long number;
            if(track==null){if(key=="Title")data.Title=value;continue;}
            if(key=="Id"&&long.TryParse(value,out number))track.Id=number;
            else if(key=="Name")track.Name=value;else if(key=="Artist")track.Artist=value;
            else if(key=="Duration"&&long.TryParse(value,out number))track.Duration=number;
        }
        data.Tracks=data.Tracks.Where(t=>t.Id>0).GroupBy(t=>t.Id).Select(g=>g.First()).Take(2000).ToList();return data;
    }
    public static int Append(ImportData added,string path){
        var existing=Load(path);int before=existing.Tracks.Count;
        existing.Tracks=existing.Tracks.Concat(added.Tracks).Where(t=>t.Id>0).GroupBy(t=>t.Id).Select(g=>g.First()).Take(2000).ToList();
        Save(existing,path);return existing.Tracks.Count-before;
    }
    public static void Save(ImportData data,string path) {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path)));
        var s=new StringBuilder("[NetEase]\r\nCount="+data.Tracks.Count+"\r\nTitle="+Clean(data.Title)+"\r\n");
        for(int i=0;i<data.Tracks.Count;i++){var t=data.Tracks[i];s.Append("\r\n[Track"+i+"]\r\nId="+t.Id+"\r\nName="+Clean(t.Name)+"\r\nArtist="+Clean(t.Artist)+"\r\nDuration="+t.Duration+"\r\n");}
        string temp=path+".new";File.WriteAllText(temp,s.ToString(),Encoding.Unicode);
        if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);
    }
}
sealed class SongPreviewList:Control {
    internal List<Track> Tracks=new List<Track>();internal HashSet<long> Chosen=new HashSet<long>();internal Action Changed;int first;bool scrolling;
    internal SongPreviewList(){SetStyle(ControlStyles.UserPaint|ControlStyles.AllPaintingInWmPaint|ControlStyles.OptimizedDoubleBuffer|ControlStyles.SupportsTransparentBackColor,true);BackColor=Color.Transparent;TabStop=true;Cursor=Cursors.Hand;}
    internal void SetTracks(List<Track> tracks){Tracks=tracks;Chosen=new HashSet<long>(tracks.Select(t=>t.Id));first=0;Invalidate();if(Changed!=null)Changed();}
    protected override void OnMouseWheel(MouseEventArgs e){base.OnMouseWheel(e);first=Math.Max(0,Math.Min(Math.Max(0,Tracks.Count-4),first+(e.Delta<0?1:-1)));Invalidate();}
    protected override void OnMouseDown(MouseEventArgs e){base.OnMouseDown(e);Focus();if(e.X>Width-18){scrolling=true;Capture=true;ScrollAt(e.Y);return;}if(e.Y/76>=4||e.Y%76>=66)return;int i=first+e.Y/76;if(i>=Tracks.Count)return;long id=Tracks[i].Id;if(!Chosen.Add(id))Chosen.Remove(id);Invalidate();if(Changed!=null)Changed();}
    void ScrollAt(int y){first=Math.Max(0,Math.Min(Math.Max(0,Tracks.Count-4),(int)Math.Round(y/(double)Math.Max(1,Height)*(Tracks.Count-4))));Invalidate();}
    protected override void OnMouseMove(MouseEventArgs e){base.OnMouseMove(e);if(scrolling)ScrollAt(e.Y);}
    protected override void OnMouseUp(MouseEventArgs e){base.OnMouseUp(e);scrolling=false;Capture=false;}
    protected override void OnPaint(PaintEventArgs e){var g=e.Graphics;g.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        if(Tracks.Count==0){using(var p=new Pen(Color.FromArgb(95,176,191,208),2)){g.DrawEllipse(p,Width/2-30,70,60,60);g.DrawEllipse(p,Width/2-8,92,16,16);}
            using(var f=new Font("Microsoft YaHei UI",13))RadioTheme.Label(g,"读取链接后，在这里选择要添加的歌曲",f,new Rectangle(20,150,Width-40,40),Color.FromArgb(174,193,210),true);return;}
        using(var title=new Font("Microsoft YaHei UI",15,FontStyle.Bold))using(var small=new Font("Microsoft YaHei UI",10)){
            int visible=Math.Min(4,Tracks.Count-first);
            for(int j=0;j<visible;++j){int i=first+j,y=j*76;var track=Tracks[i];bool chosen=Chosen.Contains(track.Id);
                using(var shape=RadioTheme.Round(new RectangleF(0,y,Width-18,66),18))using(var fill=new SolidBrush(chosen?Color.FromArgb(155,68,100,127):Color.FromArgb(100,50,71,95)))g.FillPath(fill,shape);
                using(var line=new Pen(chosen?Color.FromArgb(225,237,243):Color.FromArgb(129,154,174),2))g.DrawEllipse(line,17,y+18,28,28);
                if(chosen)using(var line=new Pen(Color.FromArgb(237,241,242),2.5f))g.DrawLines(line,new[]{new Point(24,y+32),new Point(29,y+37),new Point(38,y+26)});
                RadioTheme.Label(g,track.Name,title,new Rectangle(60,y+5,Width-170,29),Color.FromArgb(224,232,239));
                RadioTheme.Label(g,track.Artist,small,new Rectangle(62,y+34,Width-180,23),Color.FromArgb(176,196,212));
                string duration=((int)TimeSpan.FromMilliseconds(track.Duration).TotalMinutes).ToString("00")+":"+TimeSpan.FromMilliseconds(track.Duration).Seconds.ToString("00");
                RadioTheme.Label(g,duration,title,new Rectangle(Width-110,y+12,85,40),Color.FromArgb(207,222,232),true);
            }
            if(Tracks.Count>4){using(var rail=new Pen(Color.FromArgb(100,204,215,226),3))g.DrawLine(rail,Width-5,0,Width-5,Height-8);
                float h=Math.Max(22,(Height-8)*4f/Tracks.Count),y=first*(Height-8-h)/Math.Max(1,Tracks.Count-4);
                using(var thumb=new Pen(Color.FromArgb(201,213,224),5))g.DrawLine(thumb,Width-5,y,Width-5,y+h);}
        }
    }
}
sealed class ImportWindow:Form {
    readonly TextBox input=new TextBox();readonly SongPreviewList songs=new SongPreviewList();
    readonly RadioButton read=new RadioButton(),save=new RadioButton(),cancel=new RadioButton();readonly string output;readonly bool preview;ImportData data;
    string hint="粘贴歌曲或歌单分享链接",album="等待添加",summary="读取后选择歌曲，已有曲目会保留。";
    public ImportWindow(string path,bool renderPreview=false){
        preview=renderPreview;output=path;Text="随身电台 · 添加歌曲";ClientSize=new Size(1200,720);Font=new Font("Microsoft YaHei UI",11);FormBorderStyle=FormBorderStyle.None;
        StartPosition=FormStartPosition.CenterScreen;DoubleBuffered=true;BackColor=RadioTheme.Ink;Opacity=0.98;
        input.SetBounds(68,163,874,28);input.Font=new Font("Microsoft YaHei UI",13);input.BackColor=Color.FromArgb(42,65,84);input.ForeColor=RadioTheme.Text;input.BorderStyle=BorderStyle.None;Controls.Add(input);
        read.Text="读取链接";read.Font=new Font("Microsoft YaHei UI",14,FontStyle.Bold);read.SetBounds(982,151,158,48);Controls.Add(read);
        songs.SetBounds(518,291,628,306);Controls.Add(songs);
        cancel.Text="返回";cancel.Font=new Font("Microsoft YaHei UI",14,FontStyle.Bold);cancel.SetBounds(810,640,130,46);cancel.Click+=delegate{Close();};Controls.Add(cancel);
        save.Text="＋ 添加歌曲";save.Accent=true;save.Font=new Font("Microsoft YaHei UI",16,FontStyle.Bold);save.SetBounds(962,631,184,60);save.Enabled=false;Controls.Add(save);
        songs.Changed=delegate{if(data!=null){summary="已选择 "+songs.Chosen.Count+" / "+data.Tracks.Count+" 首 · 添加至网易云音源";save.Enabled=songs.Chosen.Count>0;save.Text="＋ 添加 "+songs.Chosen.Count+" 首";Invalidate();}};
        read.Click+=async delegate{try{read.Enabled=false;save.Enabled=false;hint="正在读取歌曲信息…";Invalidate();string requested=input.Text;var fetched=await NetEase.Fetch(requested);
            if(IsDisposed||Disposing)return;if(input.Text!=requested){hint="链接已变更，请重新读取";Invalidate();return;}data=fetched;album=data.Title;hint="点击曲目可选择或取消，滚轮浏览列表";songs.SetTracks(data.Tracks);Invalidate();
        }catch(Exception ex){if(!IsDisposed&&!Disposing){hint=ex.Message;Invalidate();}}finally{if(!IsDisposed&&!Disposing)read.Enabled=true;}};
        save.Click+=delegate{try{var selected=new ImportData{Title=data.Title,Tracks=data.Tracks.Where(t=>songs.Chosen.Contains(t.Id)).ToList()};int added=NetEase.Append(selected,output);summary="已添加 "+added+" 首 · 从音源菜单选择网易云即可播放";hint="添加完成，之前的列表已保留";save.Enabled=false;Invalidate();}catch(Exception ex){hint=ex.Message;Invalidate();}};
        if(preview){data=new ImportData{Title="我的导入歌单"};for(int i=0;i<5;++i)data.Tracks.Add(new Track{Id=i+1,Name="歌曲预览 · "+(i+1).ToString("00"),Artist="网易云音乐",Duration=240000+i*11000});album=data.Title;songs.SetTracks(data.Tracks);hint="点击曲目可选择或取消，滚轮浏览列表";}
        input.TextChanged+=delegate{save.Enabled=false;data=null;};KeyPreview=true;KeyDown+=delegate(object sender,KeyEventArgs e){if(e.KeyCode==Keys.Escape)Close();};
    }
    protected override bool ShowWithoutActivation{get{return preview;}}
    protected override void OnMouseUp(MouseEventArgs e){base.OnMouseUp(e);if(e.X>1115&&e.Y<105)Close();}
    protected override void OnPaintBackground(PaintEventArgs e){var g=e.Graphics;
        using(var bg=new System.Drawing.Drawing2D.LinearGradientBrush(ClientRectangle,Color.FromArgb(9,26,43),Color.FromArgb(80,73,93),90f))g.FillRectangle(bg,ClientRectangle);
        using(var glow=new System.Drawing.Drawing2D.LinearGradientBrush(new Rectangle(0,460,Width,260),Color.FromArgb(0,185,151,114),Color.FromArgb(210,188,145,104),90f))g.FillRectangle(glow,0,460,Width,260);
        using(var ellipse=new System.Drawing.Drawing2D.GraphicsPath()){ellipse.AddEllipse(500,-220,1100,1000);using(var bloom=new System.Drawing.Drawing2D.PathGradientBrush(ellipse)){bloom.CenterColor=Color.FromArgb(65,34,114,174);bloom.SurroundColors=new[]{Color.FromArgb(0,34,114,174)};g.FillPath(bloom,ellipse);}}
        using(var line=new Pen(Color.FromArgb(45,235,220,187),1.2f))for(int i=0;i<4;++i)g.DrawBezier(line,-40,640+i*18,380,470+i*25,740,730-i*15,1260,490+i*20);
    }
    static void Disc(Graphics g,float x,float y,float size,Color ink){using(var p=new Pen(ink,1.5f)){g.DrawEllipse(p,x,y,size,size);g.DrawEllipse(p,x+5,y+5,size-10,size-10);g.DrawEllipse(p,x+size*.38f,y+size*.38f,size*.24f,size*.24f);}using(var b=new SolidBrush(ink))g.FillEllipse(b,x+size*.45f,y+size*.45f,size*.1f,size*.1f);}
    protected override void OnPaint(PaintEventArgs e){base.OnPaint(e);var g=e.Graphics;g.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;
        Disc(g,42,32,51,RadioTheme.Text);
        using(var f=new Font("Microsoft YaHei UI",26,FontStyle.Bold))RadioTheme.Label(g,"添加歌曲",f,new Rectangle(108,33,720,56),Color.White);
        using(var f=new Font("Microsoft YaHei UI",11))RadioTheme.Label(g,"随身电台  /  网易云音乐",f,new Rectangle(112,90,700,26),Color.FromArgb(188,204,219));
        using(var b=new SolidBrush(RadioTheme.Text)){g.FillPolygon(b,new[]{new Point(1148,45),new Point(1156,59),new Point(1170,67),new Point(1156,75),new Point(1148,90),new Point(1140,75),new Point(1125,67),new Point(1140,59)});}
        using(var inputShape=RadioTheme.Round(new RectangleF(50,149,910,52),20))using(var fill=new SolidBrush(Color.FromArgb(42,65,84)))using(var line=new Pen(Color.FromArgb(125,166,187),1)){g.FillPath(fill,inputShape);g.DrawPath(line,inputShape);}
        using(var f=new Font("Microsoft YaHei UI",11))RadioTheme.Label(g,hint,f,new Rectangle(57,208,1080,30),Color.FromArgb(206,216,226));
        using(var f=new Font("Microsoft YaHei UI",18,FontStyle.Bold)){Disc(g,57,250,31,RadioTheme.Gold);RadioTheme.Label(g,"导入专辑",f,new Rectangle(102,246,350,43),Color.White);}
        // Album sleeve echoes the game's white paper cards without copying game artwork.
        using(var b=new SolidBrush(Color.FromArgb(90,100,128,152)))g.FillPolygon(b,new[]{new Point(151,315),new Point(444,281),new Point(444,546),new Point(151,585)});
        using(var b=new SolidBrush(Color.FromArgb(169,198,215,226)))g.FillRectangle(b,84,310,303,286);
        using(var art=new System.Drawing.Drawing2D.LinearGradientBrush(new Rectangle(91,317,289,204),Color.FromArgb(88,137,159),Color.FromArgb(16,44,68),35f))g.FillRectangle(art,91,317,289,204);
        using(var ink=new SolidBrush(Color.FromArgb(17,25,35)))g.FillEllipse(ink,144,330,176,176);Disc(g,149,335,166,Color.FromArgb(86,113,128));
        using(var ink=new SolidBrush(RadioTheme.Gold))g.FillEllipse(ink,207,393,50,50);Disc(g,218,404,28,Color.FromArgb(26,43,56));
        using(var f=new Font("Microsoft YaHei UI",18,FontStyle.Bold))RadioTheme.Label(g,data==null?"我的随身电台":album,f,new Rectangle(100,530,275,39),Color.FromArgb(12,19,24));
        using(var f=new Font("Microsoft YaHei UI",11))RadioTheme.Label(g,"NETEASE CLOUD MUSIC",f,new Rectangle(103,570,270,22),Color.FromArgb(40,53,62));
        using(var header=RadioTheme.Round(new RectangleF(512,247,634,41),19))using(var b=new SolidBrush(Color.FromArgb(165,19,22,34)))g.FillPath(b,header);
        Disc(g,523,247,40,RadioTheme.Text);using(var f=new Font("Microsoft YaHei UI",19,FontStyle.Bold))RadioTheme.Label(g,"音乐列表",f,new Rectangle(576,245,350,44),Color.White);
        using(var f=new Font("Microsoft YaHei UI",12))RadioTheme.Label(g,(data==null?0:data.Tracks.Count)+" 首",f,new Rectangle(1038,247,94,38),RadioTheme.Text,true);
        using(var footer=RadioTheme.Round(new RectangleF(32,619,1136,83),40))using(var fill=new SolidBrush(Color.FromArgb(215,27,25,36)))g.FillPath(fill,footer);
        Disc(g,53,628,65,Color.FromArgb(185,198,209));using(var f=new Font("Microsoft YaHei UI",17,FontStyle.Bold))RadioTheme.Label(g,data==null?"准备添加新音乐":album,f,new Rectangle(139,629,646,34),Color.White);
        using(var f=new Font("Microsoft YaHei UI",11))RadioTheme.Label(g,summary,f,new Rectangle(141,665,645,27),Color.FromArgb(185,197,211));
    }
}
static class Program {
    static int SelfTest() {
        string folder=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"wuwa-radio-test-"+Guid.NewGuid().ToString("N"));
        string output=Path.Combine(folder,"library.ini");
        try {
            var cases=new[]{Tuple.Create("https://music.163.com/song?id=1","song",1L),Tuple.Create("https://music.163.com/#/playlist?id=2","playlist",2L),Tuple.Create("https://y.music.163.com/m/song?id=3&tracking=discard","song",3L)};
            foreach(var c in cases){var p=NetEase.Parse(c.Item1);if(p.Item1!=c.Item2||p.Item2!=c.Item3)return 2;}
            foreach(string invalid in new[]{"https://example.com/song?id=1","https://music.163.com.evil.example/song?id=1","https://music.163.com/song?id=-1","https://music.163.com/artist?id=1"}) {
                bool rejected=false;try{NetEase.Parse(invalid);}catch{rejected=true;}if(!rejected)return 3;
            }
            var data=new ImportData{Title="Unicode test"};data.Tracks.Add(new Track{Id=42,Name="测试\r\nTitle",Artist="Synthetic artist",Duration=1000});
            NetEase.Save(data,output);string first=File.ReadAllText(output,Encoding.Unicode);
            if(!first.Contains("Count=1")||!first.Contains("Name=测试  Title")||!first.Contains("Id=42"))return 4;
            data.Tracks[0].Id=43;NetEase.Save(data,output);
            if(!File.ReadAllText(output,Encoding.Unicode).Contains("Id=43")||File.Exists(output+".new"))return 5;
            var added=new ImportData{Title="Added"};added.Tracks.Add(new Track{Id=43,Name="duplicate"});added.Tracks.Add(new Track{Id=44,Name="追加曲目",Artist="测试",Duration=2000});
            if(NetEase.Append(added,output)!=1)return 6;
            var merged=NetEase.Load(output);if(merged.Tracks.Count!=2||merged.Tracks[0].Id!=43||merged.Tracks[1].Id!=44||merged.Tracks[1].Name!="追加曲目")return 7;
            Console.WriteLine("PASS: URL parsing, host validation, Unicode metadata, atomic replacement, append preservation and ID deduplication");return 0;
        } catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}
        finally{if(File.Exists(output))File.Delete(output);if(File.Exists(output+".new"))File.Delete(output+".new");if(Directory.Exists(folder))Directory.Delete(folder);}
    }
    static string DefaultOutput() {DirectoryInfo d=new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);while(d!=null&&d.Name!="Client")d=d.Parent;if(d!=null)return Path.Combine(d.FullName,"Saved","Config","PortableRadioNetEase.ini");return Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"PortableRadioNetEase.ini");}
    [STAThread]static int Main(string[] args) {
        if(args.Length==1&&args[0]=="--self-test")return SelfTest();
        if(args.Length==2&&args[0]=="--render-preview"){
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
            using(var form=new ImportWindow(Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"preview-unused.ini"),true))
            using(var bitmap=new Bitmap(form.Width,form.Height)){form.ShowInTaskbar=false;form.StartPosition=FormStartPosition.Manual;form.Location=new Point(-30000,-30000);form.Show();Application.DoEvents();form.DrawToBitmap(bitmap,new Rectangle(0,0,bitmap.Width,bitmap.Height));bitmap.Save(Path.GetFullPath(args[1]),System.Drawing.Imaging.ImageFormat.Png);}return 0;
        }
        ServicePointManager.SecurityProtocol=SecurityProtocolType.Tls12;
        if(args.Length==3&&args[0]=="--import") {try{var data=NetEase.Fetch(args[1]).GetAwaiter().GetResult();NetEase.Save(data,args[2]);Console.WriteLine("Imported="+data.Tracks.Count);return 0;}catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}}
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new ImportWindow(args.Length==2&&args[0]=="--library"?Path.GetFullPath(args[1]):DefaultOutput()));return 0;
    }
}

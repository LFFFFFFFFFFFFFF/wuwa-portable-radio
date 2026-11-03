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
    public static void Save(ImportData data,string path) {
        Directory.CreateDirectory(Path.GetDirectoryName(Path.GetFullPath(path)));
        var s=new StringBuilder("[NetEase]\r\nCount="+data.Tracks.Count+"\r\nTitle="+Clean(data.Title)+"\r\n");
        for(int i=0;i<data.Tracks.Count;i++){var t=data.Tracks[i];s.Append("\r\n[Track"+i+"]\r\nId="+t.Id+"\r\nName="+Clean(t.Name)+"\r\nArtist="+Clean(t.Artist)+"\r\nDuration="+t.Duration+"\r\n");}
        string temp=path+".new";File.WriteAllText(temp,s.ToString(),Encoding.Unicode);
        if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);
    }
}
sealed class ImportWindow:Form {
    readonly TextBox input=new TextBox();readonly ListView songs=new ListView();readonly Label status=new Label();readonly Button read=new Button(),save=new Button();
    readonly string output;ImportData data;
    public ImportWindow(string path) {
        output=path;Text="鸣潮电台 · 导入网易云";ClientSize=new Size(690,420);Font=new Font("Microsoft YaHei UI",10);StartPosition=FormStartPosition.CenterScreen;MinimumSize=MaximumSize=Size;
        var label=new Label{Text="粘贴网易云歌曲或歌单分享链接",Left=20,Top=18,Width=620};Controls.Add(label);
        input.SetBounds(20,48,530,30);Controls.Add(input);read.Text="读取链接";read.SetBounds(560,46,110,34);Controls.Add(read);
        songs.SetBounds(20,96,650,240);songs.View=View.Details;songs.FullRowSelect=true;songs.Columns.Add("歌曲",365);songs.Columns.Add("音乐人",240);Controls.Add(songs);
        status.SetBounds(20,345,650,32);status.Text="歌曲由已登录网易云客户端播放，保留账号的播放权限。";Controls.Add(status);
        save.Text="导入为当前电台列表";save.SetBounds(420,380,250,32);save.Enabled=false;Controls.Add(save);
        read.Click+=async delegate {try{read.Enabled=false;save.Enabled=false;status.Text="正在读取歌曲信息…";data=await NetEase.Fetch(input.Text);songs.Items.Clear();foreach(var t in data.Tracks)songs.Items.Add(new ListViewItem(new[]{t.Name,t.Artist}));status.Text=data.Title+" · "+data.Tracks.Count+" 首";save.Enabled=true;}catch(Exception e){status.Text=e.Message;}finally{read.Enabled=true;}};
        save.Click+=delegate{try{NetEase.Save(data,output);status.Text="已导入。回到游戏按 F9 切到网易云，F8 暂停，F10 下一首。";}catch(Exception e){status.Text=e.Message;}};
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
            Console.WriteLine("PASS: URL parsing, host validation, Unicode metadata and atomic replacement");return 0;
        } catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}
        finally{if(File.Exists(output))File.Delete(output);if(File.Exists(output+".new"))File.Delete(output+".new");if(Directory.Exists(folder))Directory.Delete(folder);}
    }
    static string DefaultOutput() {DirectoryInfo d=new DirectoryInfo(AppDomain.CurrentDomain.BaseDirectory);while(d!=null&&d.Name!="Client")d=d.Parent;if(d!=null)return Path.Combine(d.FullName,"Saved","Config","PortableRadioNetEase.ini");return Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"PortableRadioNetEase.ini");}
    [STAThread]static int Main(string[] args) {
        if(args.Length==1&&args[0]=="--self-test")return SelfTest();
        ServicePointManager.SecurityProtocol=SecurityProtocolType.Tls12;
        if(args.Length==3&&args[0]=="--import") {try{var data=NetEase.Fetch(args[1]).GetAwaiter().GetResult();NetEase.Save(data,args[2]);Console.WriteLine("Imported="+data.Tracks.Count);return 0;}catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}}
        Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new ImportWindow(args.Length==2&&args[0]=="--library"?Path.GetFullPath(args[1]):DefaultOutput()));return 0;
    }
}

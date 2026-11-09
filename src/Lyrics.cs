using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

internal sealed class TimedLyric {
    internal long Time;
    internal string Text;
    internal TimedLyric(long time,string text){Time=time;Text=text;}
}
internal sealed class LyricDocument {
    internal List<TimedLyric> Lines=new List<TimedLyric>(),Translation=new List<TimedLyric>();
    static readonly Regex Stamp=new Regex(@"\G\[(\d{1,4}):(\d{2})(?:[.:](\d{1,3}))?\]",RegexOptions.CultureInvariant);
    static readonly Regex Offset=new Regex(@"\[offset:([+-]?\d{1,7})\]",RegexOptions.IgnoreCase|RegexOptions.CultureInvariant);
    internal static List<TimedLyric> Parse(string raw){
        if(raw==null)return new List<TimedLyric>();
        if(raw.Length>262144)throw new InvalidDataException("歌词文件过大（最多 256 KB 字符）。");
        long offset=0;foreach(Match match in Offset.Matches(raw))long.TryParse(match.Groups[1].Value,NumberStyles.Integer,CultureInfo.InvariantCulture,out offset);
        var normalized=new StringBuilder();int count=0;
        using(var reader=new StringReader(raw)){
            string line;while((line=reader.ReadLine())!=null&&count++<4000){
                line=line.TrimStart('\uFEFF',' ','\t');int cursor=0,tags=0;var prefix=new StringBuilder();
                while(tags<16){var m=Stamp.Match(line,cursor);if(!m.Success)break;
                    int minutes=int.Parse(m.Groups[1].Value,CultureInfo.InvariantCulture),seconds=int.Parse(m.Groups[2].Value,CultureInfo.InvariantCulture);
                    if(minutes>1439||seconds>59)break;
                    string fraction=m.Groups[3].Value.PadRight(3,'0');
                    prefix.AppendFormat(CultureInfo.InvariantCulture,"[{0:00}:{1:00}.{2}]",minutes,seconds,fraction);cursor+=m.Length;tags++;
                }
                if(tags==0)continue;
                normalized.Append(prefix).Append(' ').Append(line.Substring(cursor)).Append('\n');
            }
        }
        normalized.Append('\n');var result=new List<TimedLyric>();
        // Run the actual, pinned HyPlayer parser after bounding/normalizing its input.
        using(var parsed=LyricParser.Implementation.LrcParser.ParseLrc(normalized.ToString())){
            foreach(var line in parsed.Lines)result.Add(new TimedLyric((long)line.StartTime.TotalMilliseconds-offset,Clean(line.CurrentLyric)));
        }
        return result.OrderBy(x=>x.Time).GroupBy(x=>x.Time).Select(g=>new TimedLyric(g.Key,string.Join(" / ",g.Select(x=>x.Text).Distinct()))).ToList();
    }
    static string Clean(string value){var b=new StringBuilder();foreach(char c in value??""){if(!char.IsControl(c))b.Append(c);if(b.Length>=600)break;}return b.ToString().Trim();}
    internal static int Index(List<TimedLyric> lines,long position){int low=0,high=lines.Count;while(low<high){int middle=(low+high)/2;if(lines[middle].Time<=position)low=middle+1;else high=middle;}return low-1;}
    internal Tuple<string,string> At(long position){
        int index=Index(Lines,position);if(index<0)return Tuple.Create("等待第一句歌词","");
        var line=Lines[index];string translation="";
        int near=Index(Translation,line.Time+500);
        if(near>=0&&Math.Abs(Translation[near].Time-line.Time)<=500)translation=Translation[near].Text;
        return Tuple.Create(line.Text,translation);
    }
}
internal static class LyricClock {
    internal static long Position(RadioCard card,long now,int offset){
        long age=now-card.SampleTick;if(!card.Timeline||card.SampleTick<=0||age<0||age>4000||card.DurationMs<=0)return -1;
        long position=card.PositionMs+(card.Playing?age*card.RatePermille/1000:0)+offset;
        return Math.Max(0,Math.Min(card.DurationMs,position));
    }
}
internal sealed class LyricsController:IDisposable {
    readonly string folder;readonly Dictionary<string,LyricDocument> cache=new Dictionary<string,LyricDocument>();
    CancellationTokenSource pending;int revision;bool disposed;
    string selectedSource="";
    internal string Key="",Notice="等待歌曲";internal LyricDocument Document;
    internal Func<RadioCard,CancellationToken,Task<LyricDocument>> Loader;
    internal LyricsController(string config){folder=Path.Combine(Path.GetDirectoryName(config),"Lyrics");Loader=Load;}
    internal static string TrackKey(RadioCard card){
        if(card.Source=="网易云"&&card.MusicId>0)return "netease-"+card.MusicId.ToString(CultureInfo.InvariantCulture);
        if(card.Source=="本地音乐"&&!string.IsNullOrEmpty(card.LocalPath))using(var sha=SHA256.Create())return "local-"+BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(card.LocalPath.ToUpperInvariant()))).Replace("-","").Substring(0,24);
        return "";
    }
    internal string OverridePath(RadioCard card){string key=TrackKey(card);if(key.Length==0)throw new InvalidOperationException("请先播放已导入的网易云歌曲或本地音乐。");return Path.Combine(folder,key+".lrc");}
    internal void Import(RadioCard card,string file){
        string text=ReadFile(file);var parsed=LyricDocument.Parse(text);if(!parsed.Any(x=>x.Text.Length>0))throw new InvalidDataException("文件中没有带时间的 LRC 歌词。");
        Directory.CreateDirectory(folder);string target=OverridePath(card),temp=target+".new";File.WriteAllText(temp,text,new UTF8Encoding(false));
        if(File.Exists(target))File.Replace(temp,target,null);else File.Move(temp,target);Select(card,true);
    }
    internal void Select(RadioCard card,bool force=false){
        string key=TrackKey(card);if(!force&&key==Key&&selectedSource==card.Source)return;selectedSource=card.Source;
        Key=key;int request=++revision;if(pending!=null){pending.Cancel();pending.Dispose();pending=null;}
        Document=null;Notice=key.Length==0?(card.Source=="网易云"?"当前曲目未匹配导入列表":"请选择网易云或本地音乐"):"正在读取歌词…";
        if(key.Length==0)return;
        if(!force&&cache.ContainsKey(key)){Document=cache[key];Notice=Document.Lines.Count>0?"":"暂无同步歌词";return;}
        pending=new CancellationTokenSource();Fetch(card,key,request,pending.Token);
    }
    async void Fetch(RadioCard card,string key,int request,CancellationToken token){
        try{
            var document=await Task.Run(()=>Loader(card,token),token);
            if(disposed||request!=revision||token.IsCancellationRequested)return;
            Document=document;Notice=document.Lines.Count>0?"":"暂无同步歌词";if(cache.Count>=20)cache.Clear();cache[key]=document;
        }catch(OperationCanceledException){if(!disposed&&request==revision)Notice="歌词读取超时，可在字幕菜单重试";}
        catch(Exception){if(!disposed&&request==revision)Notice="歌词读取失败，可导入 LRC 或重试";}
    }
    async Task<LyricDocument> Load(RadioCard card,CancellationToken token){
        string path=OverridePath(card);var result=new LyricDocument();
        if(File.Exists(path)){result.Lines=LyricDocument.Parse(ReadFile(path));return result;}
        if(card.Source=="本地音乐"){
            path=Path.ChangeExtension(card.LocalPath,".lrc");if(File.Exists(path))result.Lines=LyricDocument.Parse(ReadFile(path));
            string translated=Path.ChangeExtension(card.LocalPath,".zh.lrc");if(File.Exists(translated))result.Translation=LyricDocument.Parse(ReadFile(translated));return result;
        }
        return await FetchNetEase(card.MusicId,token);
    }
    internal static string ReadFile(string path){
        using(var stream=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.ReadWrite)){
            if(stream.Length>1048576)throw new InvalidDataException("歌词文件超过 1 MB。");
            using(var reader=new StreamReader(stream,Encoding.UTF8,true))return reader.ReadToEnd();
        }
    }
    internal static LyricDocument ParseNetEase(string json){
        var d=new JavaScriptSerializer{MaxJsonLength=1048576}.DeserializeObject(json) as Dictionary<string,object>;
        if(d==null||!d.ContainsKey("code")||Convert.ToInt32(d["code"],CultureInfo.InvariantCulture)!=200)throw new InvalidDataException("歌词接口没有返回成功结果。");
        return new LyricDocument{Lines=LyricDocument.Parse(Text(d,"lrc")),Translation=LyricDocument.Parse(Text(d,"tlyric"))};
    }
    static string Text(Dictionary<string,object> data,string key){object value;if(!data.TryGetValue(key,out value))return "";var part=value as Dictionary<string,object>;return part!=null&&part.TryGetValue("lyric",out value)?Convert.ToString(value,CultureInfo.InvariantCulture):"";}
    internal static async Task<LyricDocument> FetchNetEase(long id,CancellationToken token){
        if(id<=0)throw new ArgumentException("Missing confirmed song ID");
        using(var handler=new HttpClientHandler{AutomaticDecompression=DecompressionMethods.GZip|DecompressionMethods.Deflate,AllowAutoRedirect=false,UseCookies=false})
        using(var client=new HttpClient(handler))using(var budget=CancellationTokenSource.CreateLinkedTokenSource(token)){
            budget.CancelAfter(10000);token=budget.Token;
            client.Timeout=TimeSpan.FromSeconds(10);client.DefaultRequestHeaders.UserAgent.ParseAdd("PortableRadio/2.8");client.DefaultRequestHeaders.Referrer=new Uri("https://music.163.com/");
            string url="https://music.163.com/api/song/lyric?id="+id.ToString(CultureInfo.InvariantCulture)+"&lv=-1&tv=-1&kv=-1";
            using(var response=await client.GetAsync(url,HttpCompletionOption.ResponseHeadersRead,token)){
                response.EnsureSuccessStatusCode();if(response.Content.Headers.ContentLength>1048576)throw new InvalidDataException("Lyric response too large");
                using(var source=await response.Content.ReadAsStreamAsync())using(var data=new MemoryStream()){
                    var buffer=new byte[8192];int read;while((read=await source.ReadAsync(buffer,0,buffer.Length,token))>0){if(data.Length+read>1048576)throw new InvalidDataException("Lyric response too large");data.Write(buffer,0,read);}
                    return ParseNetEase(Encoding.UTF8.GetString(data.ToArray()));
                }
            }
        }
    }
    public void Dispose(){disposed=true;++revision;if(pending!=null){pending.Cancel();pending.Dispose();}cache.Clear();}
}
internal sealed class LyricsWindow:Form {
    internal string Line="",Translation="",Note="随身电台 · 歌曲字幕";
    internal LyricsWindow(){Text="随身电台 · 歌曲字幕";FormBorderStyle=FormBorderStyle.None;ShowInTaskbar=false;TopMost=true;DoubleBuffered=true;AutoScaleMode=AutoScaleMode.None;ClientSize=new Size(960,116);BackColor=Color.Magenta;TransparencyKey=Color.Magenta;Opacity=0.95;}
    protected override bool ShowWithoutActivation{get{return true;}}
    protected override CreateParams CreateParams{get{var p=base.CreateParams;p.ExStyle|=0x08000000|0x80|0x80000|0x20;return p;}}
    internal static void PaintSubtitle(Graphics g,Rectangle bounds,string line,string translation,string note){
        g.SmoothingMode=SmoothingMode.AntiAlias;
        using(var shape=RadioTheme.Round(new RectangleF(1,1,bounds.Width-3,bounds.Height-3),22))using(var fill=new SolidBrush(Color.FromArgb(12,28,42)))using(var edge=new Pen(Color.FromArgb(56,127,147),1)){g.FillPath(fill,shape);g.DrawPath(edge,shape);}
        using(var gold=new SolidBrush(RadioTheme.Gold))g.FillRectangle(gold,23,20,3,14);
        using(var font=new Font("Microsoft YaHei UI",9))RadioTheme.Label(g,note,font,new Rectangle(36,12,bounds.Width-62,24),Color.FromArgb(170,196,208),true);
        int size=22;using(var measure=new Font("Microsoft YaHei UI",size,FontStyle.Bold)){if(g.MeasureString(line,measure).Width>bounds.Width-70)size=17;}
        using(var main=new Font("Microsoft YaHei UI",size,FontStyle.Bold))RadioTheme.Label(g,line,main,new Rectangle(24,37,bounds.Width-48,38),Color.FromArgb(239,243,245),true);
        using(var sub=new Font("Microsoft YaHei UI",12))RadioTheme.Label(g,translation,sub,new Rectangle(28,78,bounds.Width-56,25),Color.FromArgb(139,215,226),true);
    }
    protected override void OnPaint(PaintEventArgs e){e.Graphics.Clear(Color.Magenta);PaintSubtitle(e.Graphics,ClientRectangle,Line,Translation,Note);}
}

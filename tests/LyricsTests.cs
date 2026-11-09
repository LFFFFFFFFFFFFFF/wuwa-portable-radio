using System;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

internal static class LyricsTests {
    static void Check(bool value,string name){if(!value)throw new Exception(name);}
    internal static int Run(){try{
        var lines=LyricDocument.Parse("[ti:Synthetic]\n[00:01.2][00:03.45]第一句\n[00:04]末句");
        Check(lines.Count==3&&lines[0].Time==1200&&lines[1].Time==3450&&lines[2].Time==4000&&lines[2].Text=="末句","fractions / repeated timestamps / final line");
        Check(LyricDocument.Parse("[00:01]早\n[offset:+250]")[0].Time==750,"global positive offset at end");
        Check(LyricDocument.Parse("[offset:-250]\n[00:01]晚")[0].Time==1250,"negative offset");
        var blank=LyricDocument.Parse("[00:01]唱句\n[00:02]\n[99:99]无效\n[bad]忽略");Check(blank.Count==2&&blank[1].Text=="","blank clears lyrics and invalid tags ignored");
        var doc=new LyricDocument{Lines=LyricDocument.Parse("[00:01]甲\n[00:01]乙\n[00:03]丙"),Translation=LyricDocument.Parse("[00:01]A\n[00:03]C")};
        Check(doc.At(999).Item1=="等待第一句歌词"&&doc.At(1000).Item1=="甲 / 乙"&&doc.At(1000).Item2=="A"&&doc.At(3500).Item1=="丙"&&doc.At(1100).Item1=="甲 / 乙","line lookup, translation, forward and backward seek");
        bool bounded=false;try{LyricDocument.Parse(new string('x',262145));}catch(InvalidDataException){bounded=true;}Check(bounded,"input bound");
        var network=LyricsController.ParseNetEase("{\"code\":200,\"lrc\":{\"lyric\":\"[00:01]Synthetic\"},\"tlyric\":{\"lyric\":\"[00:01]测试\"}}");Check(network.Lines.Count==1&&network.Translation.Count==1,"NetEase envelope");
        Check(LyricsController.ParseNetEase("{\"code\":200,\"nolyric\":true}").Lines.Count==0,"instrumental/no lyrics");
        var card=new RadioCard{Source="网易云",MusicId=1,Timeline=true,Playing=true,PositionMs=1000,DurationMs=10000,SampleTick=5000};
        Check(LyricClock.Position(card,5500,0)==1500,"playing clock");card.Playing=false;Check(LyricClock.Position(card,5500,0)==1000,"paused clock");
        card.Playing=true;card.RatePermille=2000;Check(LyricClock.Position(card,5500,250)==2250,"playback rate and adjustment");Check(LyricClock.Position(card,10000,0)==-1,"stale clock rejected");card.Timeline=false;Check(LyricClock.Position(card,5500,0)==-1,"no invented progress");
        string config=Path.Combine(Path.GetTempPath(),"portable-radio-lyrics-fixture.ini");
        using(var coordinator=new LyricsController(config)){
            var old=new TaskCompletionSource<LyricDocument>();var current=new TaskCompletionSource<LyricDocument>();
            coordinator.Loader=(c,t)=>c.MusicId==1?old.Task:current.Task;
            coordinator.Select(new RadioCard{Source="网易云",MusicId=1});coordinator.Select(new RadioCard{Source="网易云",MusicId=2});current.SetResult(network);
            Check(SpinWait.SpinUntil(()=>coordinator.Document==network,2000),"new track completed");old.SetResult(doc);Thread.Sleep(50);Check(coordinator.Document==network,"late old-song fetch discarded");
        }
        string temporary=AppDomain.CurrentDomain.BaseDirectory;
        string local=Path.Combine(temporary,"lyrics-fixture-"+Guid.NewGuid().ToString("N")+"-测试歌曲.mp3"),lrc=Path.ChangeExtension(local,".lrc"),translated=Path.ChangeExtension(local,".zh.lrc");
        try{File.WriteAllText(lrc,"[00:01]本地测试",System.Text.Encoding.Unicode);File.WriteAllText(translated,"[00:01]Local fixture",System.Text.Encoding.UTF8);
            using(var reader=new LyricsController(Path.Combine(temporary,"config.ini"))){var localDoc=reader.Loader(new RadioCard{Source="本地音乐",LocalPath=local},CancellationToken.None).GetAwaiter().GetResult();Check(localDoc.Lines.Count==1&&localDoc.Translation.Count==1,"adjacent Unicode LRC and translation files");}
        }finally{File.Delete(lrc);File.Delete(translated);}
        using(var bitmap=new Bitmap(960,116))using(var g=Graphics.FromImage(bitmap))LyricsWindow.PaintSubtitle(g,new Rectangle(0,0,960,116),new string('歌',300),"翻译 · test","随身电台 · 字幕");
        Console.WriteLine("PASS: pinned HyPlayer parser, Unicode/fractions/multi-tag/offsets/blank lines, translation, seeks, paused/stale/rate clocks, bounded data, network envelope and stale-track fetch rejection");return 0;
    }catch(Exception ex){Console.Error.WriteLine(ex);return 1;}}
    internal static int Probe(string value){try{long id=long.Parse(value);System.Net.ServicePointManager.SecurityProtocol=System.Net.SecurityProtocolType.Tls12;var result=LyricsController.FetchNetEase(id,CancellationToken.None).GetAwaiter().GetResult();Console.WriteLine("SyncedLines="+result.Lines.Count+" TranslationLines="+result.Translation.Count);return result.Lines.Any(x=>x.Text.Length>0)?0:2;}catch(Exception ex){Console.Error.WriteLine(ex.GetType().Name);return 1;}}
    internal static void Render(string path){using(var bitmap=new Bitmap(1000,290))using(var g=Graphics.FromImage(bitmap)){
        g.Clear(Color.FromArgb(7,19,34));using(var font=new Font("Microsoft YaHei UI",12))g.DrawString("歌曲字幕 · 游戏风格预览（以下为合成测试文本）",font,Brushes.LightGray,22,15);
        using(var card=new Bitmap(960,116))using(var cg=Graphics.FromImage(card)){cg.Clear(Color.FromArgb(7,19,34));LyricsWindow.PaintSubtitle(cg,new Rectangle(0,0,960,116),"穿过夜色，让旋律陪我前行","Let the melody follow the road ahead.","随身电台 · 网易云 · 同步字幕");g.DrawImageUnscaled(card,20,55);}
        using(var font=new Font("Microsoft YaHei UI",11))g.DrawString("右键播放条 → 歌曲字幕 / 字幕设置   ·   暂停冻结   ·   切歌自动更新   ·   可导入 LRC",font,Brushes.LightGray,30,210);
        bitmap.Save(path,System.Drawing.Imaging.ImageFormat.Png);
    }}
}

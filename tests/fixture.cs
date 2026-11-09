// Synthetic audio/controller lifecycle. Contains no game code or actual audio.
using System.Runtime.CompilerServices;
public sealed class VehiclePassengerInfo { }
// Handwritten scene lifecycle used to verify attachment without any vehicle event.
public sealed class GameModeModel {
    private static readonly GameModeModel _instance=new GameModeModel();
    public static GameModeModel Instance{get{return _instance;}}
    public bool Ready,IsLoading,HasData,FailRead;
    public bool WorldDoneAndLoadingClosed{get{if(FailRead)throw new System.InvalidOperationException("Synthetic scene failure");return Ready;}}
    public bool Loading{get{return IsLoading;}}
    public bool HasGameModeData{get{return HasData;}}
}
public sealed class GameModeController {
    public int Ticks;
    public float LastDelta;
    [MethodImpl(MethodImplOptions.NoInlining)]
    public void OnTick(float delta){LastDelta=delta;Ticks++;}
}
public sealed class MotorcycleMusicPlayerController {
    private static MotorcycleMusicPlayerController _Instance;
    public static MotorcycleMusicPlayerController Instance { get { return _Instance; } }
    public MotorcycleMusicPlayerController() { _Instance=this;MotorcycleMusicPlayerModel.EnsureCreated(); }
    public int CurHandleId = -1;
    public bool IsPlaying;
    public bool IsInVehicle;
    public int Pauses, NextTracks, Stops, SeatChanges;
    public int PreviousRequests,NextRequests,OpenedViews;
    [MethodImpl(MethodImplOptions.NoInlining)]
    public bool CheckIsEnable(bool tips){return MotorcycleMusicPlayerModel.Instance.IsEnable&&IsInVehicle;}
    public void QuickPlayMusic(bool next){
        if(!CheckIsEnable(true))return;
        if(next)NextRequests++;else PreviousRequests++;
        var model=MotorcycleMusicPlayerModel.Instance;
        int[] queue={501,502,504};int index=System.Array.IndexOf(queue,model.CurPlayMusicId);
        model.CurPlayMusicId=queue[(index+(next?1:queue.Length-1)+queue.Length)%queue.Length];IsPlaying=true;
    }
    public void OpenMusicPlayerView(){if(CheckIsEnable(true)){OpenedViews++;CSharpScript.Game.Ui.UiManager.Instance.Visible=true;}}
    [MethodImpl(MethodImplOptions.NoInlining)]
    public void OnEnterVehicle(VehiclePassengerInfo vehicle,bool seatChange) { IsInVehicle=true; if(seatChange)SeatChanges++; }
    public void PauseMusic() { IsPlaying=false; }
    public void ResumeMusic() { if(CurHandleId!=-1)IsPlaying=true; }
    [MethodImpl(MethodImplOptions.NoInlining)]
    public void OnLeaveVehicle(VehiclePassengerInfo vehicle, bool seatChange) {
        if (seatChange) SeatChanges++;
        IsInVehicle = false;
        IsPlaying = false;
        Pauses++;
    }
    public void StopMusic() { CurHandleId = -1; IsPlaying = false; Stops++; }
    public void OnBackLoginView() { StopMusic(); }
    public bool OnClear() { StopMusic(); return true; }
    public void OnMusicEnd() { if (IsPlaying && IsInVehicle) NextTracks++; }
}
namespace CSharpScript.Game.Ui {
    public struct EUiViewName {
        public string Value;
        public static readonly EUiViewName MotorcycleMusicPlayerView=new EUiViewName{Value="MotorcycleMusicPlayerView"};
    }
    public sealed class UiManager {
        private static readonly UiManager _Instance=new UiManager();
        public static UiManager Instance{get{return _Instance;}}
        public bool Visible;
        public bool IsViewShow(EUiViewName name){return Visible&&name.Value=="MotorcycleMusicPlayerView";}
    }
}

// Handwritten metadata contract, including nullable struct returns and localization.
public sealed class MotorcycleMusicPlayerModel {
    private static MotorcycleMusicPlayerModel _Instance;
    public static MotorcycleMusicPlayerModel Instance { get { return _Instance; } }
    public int CurPlayMusicId=501;
    public bool UseAlternateLanguage;
    public bool Enabled=true;
    public bool IsEnable{get{return Enabled;}}
    public static void EnsureCreated(){if(_Instance==null)_Instance=new MotorcycleMusicPlayerModel();}
}
public sealed class PhonographConfig {
    private static readonly PhonographConfig _Instance=new PhonographConfig();
    public static PhonographConfig Instance { get { return _Instance; } }
    public Aki.Config.PhonographMusic? GetMusicById(int id) {
        if(id==999)throw new System.InvalidOperationException("Synthetic config failure");
        if(id==501)return new Aki.Config.PhonographMusic("fixture-a");
        if(id==502)return new Aki.Config.PhonographMusic("fixture-b");
        if(id==998)return new Aki.Config.PhonographMusic("fixture-error");
        return null;
    }
}
// Synthetic AudioSystem contract. No real engine, audio device or volume changes.
public sealed class AudioSystem {
    private static readonly AudioSystem _instance=new AudioSystem();
    public static AudioSystem Instance{get{return _instance;}}
    public int StateCalls;
    public bool RadioState,FailState;
    public float VoiceVolume=0.7f,EffectsVolume=0.8f,RadioVolume=0.6f;
    public void SetState(string group,string state,bool validate){
        if(FailState)throw new System.InvalidOperationException("Synthetic routing failure");
        if(!validate||group!="system_motor_radio"||(state!="playing"&&state!="none"))throw new System.ArgumentException("Unexpected audio state");
        RadioState=state=="playing";StateCalls++;
    }
}
namespace Aki.Config {
    public struct PhonographMusic {
        private string title;
        public PhonographMusic(string value){title=value;}
        public string Title { get { return title; } }
    }
    public static class ConfigMultiTextLang {
        public static string GetLocalTextNew(string key,string language) {
            if(language!=null)throw new System.ArgumentException("Expected game's null language parameter");
            if(key=="fixture-error")throw new System.InvalidOperationException("Synthetic translation failure");
            if(MotorcycleMusicPlayerModel.Instance.UseAlternateLanguage)return key=="fixture-a"?"Native track A":"Native track B";
            return key=="fixture-a"?"测试原生曲目甲":key=="fixture-b"?"测试原生曲目乙":null;
        }
    }
}

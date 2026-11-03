// Synthetic audio/controller lifecycle. Contains no game code or actual audio.
using System.Runtime.CompilerServices;
public sealed class VehiclePassengerInfo { }
public sealed class MotorcycleMusicPlayerController {
    private static MotorcycleMusicPlayerController _Instance;
    public static MotorcycleMusicPlayerController Instance { get { return _Instance; } }
    public MotorcycleMusicPlayerController() { _Instance=this;MotorcycleMusicPlayerModel.EnsureCreated(); }
    public int CurHandleId = -1;
    public bool IsPlaying;
    public bool IsInVehicle;
    public int Pauses, NextTracks, Stops, SeatChanges;
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

// Handwritten metadata contract, including nullable struct returns and localization.
public sealed class MotorcycleMusicPlayerModel {
    private static MotorcycleMusicPlayerModel _Instance;
    public static MotorcycleMusicPlayerModel Instance { get { return _Instance; } }
    public int CurPlayMusicId=501;
    public bool UseAlternateLanguage;
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

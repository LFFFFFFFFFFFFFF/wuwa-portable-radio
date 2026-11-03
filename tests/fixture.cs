// Synthetic audio/controller lifecycle. Contains no game code or actual audio.
using System.Runtime.CompilerServices;
public sealed class VehiclePassengerInfo { }
public sealed class MotorcycleMusicPlayerController {
    private static MotorcycleMusicPlayerController _Instance;
    public static MotorcycleMusicPlayerController Instance { get { return _Instance; } }
    public MotorcycleMusicPlayerController() { _Instance=this; }
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

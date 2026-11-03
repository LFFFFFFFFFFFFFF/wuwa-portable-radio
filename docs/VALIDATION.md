# Validation boundaries

| Area | Evidence | Limit |
| --- | --- | --- |
| Native radio continuation | User confirmed music continues after dismount on one audited client | Other client versions are not established |
| ASI entry | Actual local OptiScaler late-loaded the module | Depends on the installed loader and its configuration |
| Native control bridge | Standalone host, locally supplied Mono runtime, handwritten fixture; pause/resume, paused dismount, source-state restoration, cleanup, repeated calls and unhooking tested | Synthetic behavior does not prove live game hotkeys |
| NetEase import | A song and a 200-track public list were imported; foreign-host input rejected | Public metadata endpoints can change; private lists not supported |
| NetEase transport controls | Targeted Windows SMTC pause/resume accepted and playback state restored | Requires the official client, SMTC and account access |
| Local player | Muted generated WAV/MP3 files; Unicode paths, corrupt-file skip, pause/resume and next-track behavior tested | Not an assurance for every codec/file |
| New game hotkeys | Awaiting live user confirmation | Do not describe these as fully game-validated |

The repository contains no extracted game code, game assembly, runtime binary, songs, account cookies or raw game logs.

## Repeatable checks

```powershell
python scripts/check_source.py
pwsh -NoProfile -File scripts/build.ps1 -WithTests
```

The importer self-test does not access the network or read the user's library.

For optional integration experiments, independently provide the compatible runtime and its base-class-library directory from your own installation. Do not commit or upload those files:

```powershell
build/radio_lab.exe "<coreclr.dll>" "<base-class-library-directory>" "<absolute-build-path>/Aki.Game.dll"
build/radio_loader_lab.exe "<coreclr.dll>" "<base-class-library-directory>" "<absolute-build-path>/Aki.Game.dll" "<absolute-build-path>/PortableRadioTest.asi"
```

`build/Aki.Game.dll` in these commands is generated solely from `tests/fixture.cs`. It is a synthetic fixture sharing the target type names, not the proprietary game assembly. The test ASI is built with `RADIO_LAB` and must never be installed into the game.

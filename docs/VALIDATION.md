# Validation boundaries

| Area | Evidence | Limit |
| --- | --- | --- |
| Native radio continuation | User confirmed music continues after dismount on one audited client | Other client versions are not established |
| ASI entry | Actual local OptiScaler late-loaded the module | Depends on the installed loader and its configuration |
| Native control bridge | Standalone host, locally supplied Mono runtime, handwritten fixture; pause/resume, paused dismount, source-state restoration, cleanup, repeated calls and unhooking tested | Synthetic behavior does not prove live game hotkeys |
| Native track names | Static metadata audit identified model track ID, nullable music config/title and localization signatures. Local Mono fixture tests exercised actual nullable boxing/struct getters, localized Unicode names, next track, pause, wrong-thread rejection, missing data, config/translation exceptions, recovery, queued refresh and session reset | Read-only metadata queries use the observed game thread and add no detours. Actual game titles still await live confirmation |
| NetEase import | A song and a 200-track public list were imported; foreign-host input rejected | Public metadata endpoints can change; private lists not supported |
| NetEase transport controls | Targeted Windows SMTC pause/resume accepted and playback state restored | Requires the official client, SMTC and account access |
| Local player | Muted generated WAV/MP3 files; Unicode paths, corrupt-file skip, pause/resume and next-track behavior tested | Not an assurance for every codec/file |
| New game hotkeys | Awaiting live user confirmation | Do not describe these as fully game-validated |
| Now-playing display | UTF-16/Unicode/PID/parser/long-title render tests; card preview visually inspected | New foreground-only overlay still needs live game confirmation; exclusive fullscreen may hide it |
| Actual vs requested title | Native unit tests cover pending/timeout/confirmation, equivalent NFC/NFD Japanese titles, paused state, natural next, an unrelated client queue, missing SMTC and same-title/different-artist input | A metadata match is title/artist matching, not an authenticated NetEase track-ID acknowledgement |
| Metadata watcher | Dedicated COM thread, bounded async waits; a read-only live probe is supplied | Latest local run had no active NetEase media session; actual new metadata path still needs an active client test |

The repository contains no extracted game code, game assembly, runtime binary, songs, account cookies or raw game logs.

Native metadata calls follow the [Mono embedding API](https://www.mono-project.com/docs/advanced/embedding/). Temporary managed results use short-lived GC handles; no managed object/string pointers are passed to the HUD worker. The HUD receives a copied UTF-16 snapshot. Refresh messages have separate bookkeeping and do not consume playback command acknowledgements.

## Repeatable checks

```powershell
python scripts/check_source.py
pwsh -NoProfile -File scripts/build.ps1 -WithTests
```

The importer and HUD self-tests do not access the network or read the user's library. With `-WithTests`, the build also runs `track_info_lab.exe` without network access. `cloud_status_lab.exe` is an optional, read-only local probe: it reports metadata lengths/playback state and sends no playback commands. It returns 2 if there is no NetEase session. It is not run by CI.

The display uses Windows [SMTC media properties](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssession) and a separate [non-activating, layered window](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles). It does not add rendering hooks. `RadioHud.exe --render-preview <output.png>` renders a synthetic preview without opening a game window.

For optional integration experiments, independently provide the compatible runtime and its base-class-library directory from your own installation. Do not commit or upload those files:

```powershell
build/radio_lab.exe "<coreclr.dll>" "<base-class-library-directory>" "<absolute-build-path>/Aki.Game.dll"
build/radio_loader_lab.exe "<coreclr.dll>" "<base-class-library-directory>" "<absolute-build-path>/Aki.Game.dll" "<absolute-build-path>/PortableRadioTest.asi"
```

`build/Aki.Game.dll` in these commands is generated solely from `tests/fixture.cs`. It is a synthetic fixture sharing the target type names, not the proprietary game assembly. The test ASI is built with `RADIO_LAB` and must never be installed into the game.

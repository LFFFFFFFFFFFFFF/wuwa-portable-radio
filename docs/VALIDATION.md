# Validation boundaries

| Area | Evidence | Limit |
| --- | --- | --- |
| Native radio continuation | User confirmed music continues after dismount on one audited client | Other client versions are not established |
| ASI entry | Actual local OptiScaler late-loaded the module | Depends on the installed loader and its configuration |
| Native control bridge | Standalone host, locally supplied Mono runtime, handwritten fixture; pause/resume, paused dismount, source-state restoration, cleanup, repeated calls and unhooking tested | Synthetic behavior does not prove live game hotkeys |
| Native track names | Static metadata audit identified model track ID, nullable music config/title and localization signatures. Local Mono fixture tests exercised actual nullable boxing/struct getters, localized Unicode names, next track, pause, wrong-thread rejection, missing data, config/translation exceptions, recovery, queued refresh and session reset | Read-only metadata queries use the observed game thread and add no detours. The local v2.3 game log recorded successful name resolution; the expanded v2.5 UI/controls still require live confirmation |
| NetEase import | A song and a 200-track public list were imported; foreign-host input rejected | Public metadata endpoints can change; private lists not supported |
| NetEase transport controls | Targeted Windows SMTC pause/resume accepted and playback state restored | Requires the official client, SMTC and account access |
| Local player | Muted generated WAV/MP3 files; Unicode paths, corrupt-file skip, pause/resume and next-track behavior tested | Not an assurance for every codec/file |
| New keys and native navigation | Key parser tests cover minus/equals/OEM_3 and disabled legacy defaults. Local Mono fixture exercised both QuickPlayMusic directions and the official view opener | New bindings/native navigation still need live confirmation |
| UI and add-song entry | Compact player, native-view add-on and importer rendered and visually inspected. Synthetic UI manager exercised a boxed view-name value and IsViewShow, including session reset | Actual game add-on placement and click transport still need live confirmation; exclusive fullscreen may hide it |
| Actual vs requested title | Native unit tests cover pending/timeout/confirmation, equivalent NFC/NFD Japanese titles, paused state, natural next, an unrelated client queue, missing SMTC and same-title/different-artist input | A metadata match is title/artist matching, not an authenticated NetEase track-ID acknowledgement |
| Metadata watcher | Dedicated COM thread, bounded async waits; a read-only live probe is supplied | Latest local run had no active NetEase media session; actual new metadata path still needs an active client test |
| Background routing | Local Mono fixture tests cover MVID rejection, playing/paused/unavailable state requests, option-off release, preserving active native routing, wrong-thread rejection, restore failures/retry and scene cleanup; synthetic speech/SFX/radio volume fields stay unchanged | This verifies calls and lifecycle, not actual acoustic isolation. The game mixing rules and new 3.7.18 live behavior still need confirmation |
| 3.7.18 compatibility | Current installed resource DLLs were extracted read-only with entry hash verification, MVIDs/signatures/fields audited and core control IL compared. The runtime binary hash is unchanged | No general future-version guarantee; original cleanup body is still called in full |
| Automatic scene attachment | Audited game-mode tick and scene getter contracts. Local Mono fixture and full ASI loader tests activate controls without a vehicle event; loading/missing-data guards, getter exceptions, BGM release, re-entry and original tick arguments/body are checked | New scene lifecycle integration still needs a live game test; delayed initialization remains |
| On-foot official view/navigation | Audited CheckIsEnable and native model IsEnable contracts. Fixture opens the view and requests previous/next before any vehicle event, while the riding field remains false; disabled native model and inactive scenes are rejected | Actual new native-view behavior still needs live confirmation; no unlock/catalog state is modified |
| Append import | Network-free merge tests preserve old tracks, deduplicate by ID and append new songs; new UI supports selecting individual tracks | Adds to the external NetEase library, not the official unlock/catalog data |

The repository contains no extracted game code, game assembly, runtime binary, songs, account cookies or raw game logs.

v2.8 adds network-free subtitle tests for the pinned HyPlayer LRC parser,
fractional/multiple timestamps, signed global offsets, blank-line clearing,
translations, forward/backward seeks, pause/rate/stale clocks, adjacent UTF-8/
UTF-16 files, bounded data and superseded fetches. A read-only request for the
user-supplied NetEase ID returned 65 timed original lines and 62 translation lines;
only counts were recorded. The synthetic subtitle preview was rendered and
visually checked. Real client-to-game subtitle timing remains to be tested.

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

# Wuthering Waves Portable Radio

[中文](README.md)

An experimental Windows x64 portable radio with source switching, playback controls, NetEase song/playlist link imports and a now-playing display. Keeping motorbike music playing after dismounting is one of its features.

**Status:** dismount continuation was tested on one 3.7 client. Link import and NetEase client media controls were tested separately. The newer hotkeys and now-playing overlay still await live game confirmation. This is not a general compatibility guarantee.

## Controls

| Key | Action |
| --- | --- |
| F7 | Show current source, title, artist and playback state |
| F8 | Pause/resume the current source |
| F9 | Switch between the native radio and NetEase |
| F10 | Next item in the imported list |
| F11 | Open the link importer |

Only the foreground game responds. Enter and leave the motorbike once after initialization so the plugin can observe the controller thread.

Track/source changes show a six-second card in the top-right corner. F7 shows it again. NetEase titles come from the client's actual SMTC session, including natural queue changes; requested tracks remain explicitly unconfirmed until matching metadata arrives. Local tracks show filenames.

The native motorbike source also reads the current title and playback state. After observing a motorbike enter/leave event, the plugin reads the current track ID on that same game thread and resolves its configuration title through the game's localization API. It checks for track changes about once per second; F7 forces a refresh. Missing configuration/translation clears the old title and shows a track-ID fallback. This metadata path has been tested with synthetic controllers and still awaits live game confirmation. A metadata-contract mismatch disables title lookup independently of playback hooks.

The separate `RadioHud.exe` display does not activate or intercept mouse input. It hides outside the foreground game and exits when the game closes. Windowed/borderless modes are supported; exclusive fullscreen visibility is not guaranteed. Configure `[HUD] Enabled=0`, `AlwaysVisible=1`, or `ShowSeconds=6` as desired. Existing configurations use these defaults without modification.

NetEase playback is delegated to the logged-in official desktop client, with SMTC enabled under its system settings. Imports contain song/public-playlist metadata. Account playback permissions remain enforced by the client. No credentials, encrypted audio, cookies, or game files are distributed.

F10 follows the imported list; natural track-end behavior follows the NetEase client's own queue. Set `Source=Local` to use the optional MP3/WAV folder player instead.

## Build

Install PowerShell 7, Visual Studio Build Tools with Desktop development with C++, a Windows 10/11 SDK with C++/WinRT, and the .NET Framework 4.x compiler.

```powershell
pwsh -NoProfile -File scripts/build.ps1
```

The compiler and SDK are discovered automatically. Outputs are in `build/`. The importer runs network-free self-tests during the build. `-WithTests` additionally builds synthetic fixtures and native test harnesses. See [validation](docs/VALIDATION.md).

## Install

Use a separately installed, working [OptiScaler](https://github.com/optiscaler/OptiScaler) build with ASI support. Do not overwrite an existing graphics proxy DLL.

With the game fully closed, back up `OptiScaler.ini`, copy the built ASI, importer, `RadioHud.exe` and MinHook notice into `Client/Binaries/Win64/plugins/`, enable `[Plugins] LoadAsiPlugins=true` and select that directory as its plugin path. Review other ASI files before enabling the directory. Upgrades must include the new HUD executable.

Copy `PortableRadio.ini.example` to `Client/Saved/Config/PortableRadio.ini`. Use UTF-16 if adding non-ASCII paths. Keep the official NetEase client logged in with SMTC enabled, then start the game normally.

The code requires the audited assembly MVID `398499d1-a649-4e50-8ac2-270d7cbbc858`, method token, signatures, field types and a verified distinct JIT body. Mismatches refuse installation. Do not remove compatibility checks to force support for another client.

F11 opens the importer: paste a song or public-playlist share link, preview the tracks, then replace the Mod's current list. Switch sources again to reload it. Standalone importer usage can pass `--library "<path>"`.

Configuration and list files are under `Client/Saved/Config/`; diagnostics are under `Client/Saved/Logs/PortableRadioNative.log`. The display reads an ephemeral `PortableRadioStatus-<pid>.ini` there; this local listening state is never uploaded. Public NetEase web endpoints can change and may need maintenance. Private account playlists are not supported.

To uninstall, close the game, remove this project's ASI/importer/HUD executable, and restore your backed-up loader settings. Keep your own music and configuration files unless you intend to delete them.

Original code is [MIT licensed](LICENSE). MinHook retains its original license; see [third-party notices](THIRD_PARTY_NOTICES.md). This project is not affiliated with Kuro Games, NetEase, or OptiScaler.

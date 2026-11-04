# Wuthering Waves Portable Radio

[中文](README.md)

An experimental Windows x64 portable radio with source switching, playback controls, NetEase song/playlist link imports and a now-playing display. Keeping motorbike music playing after dismounting is one of its features.

**Status:** dismount continuation was tested on one 3.7 client. Link import and NetEase client media controls were tested separately. The newer hotkeys and now-playing overlay still await live game confirmation. This is not a general compatibility guarantee.

## Controls and UI

| Key | Action |
| --- | --- |
| `-` | Previous track, including the official native navigation function |
| `=` | Next track, including the official native navigation function |
| Backtick / the key below Esc | Open the original in-game radio view |

Hotkeys respond only in the foreground game, without Ctrl/Alt/Shift. Legacy F7–F11 bindings are disabled by default. Enter/leave the motorbike once after initialization to observe its controller thread.

The full album/song screen is the game's original view. A slim cyan player replaces the old diagnostic card; it offers previous/pause/next buttons and a right-click source menu. During riding, the official native mini-player is retained. When the full radio view is visible, an aligned add-on provides **Add songs** and **Source** buttons beside the channel title. It is a separate window, not a redistributed or overwritten game UI asset.

The game-styled add-song panel uses a vinyl sleeve, translucent track rows and a rounded footer; users can select individual tracks. It accepts NetEase song/public-playlist links and previews the metadata. Adding preserves the existing external list and deduplicates by song ID; it does not create or unlock official game songs. Choose NetEase from the source menu to play. The official logged-in client remains responsible for audio and account permissions; SMTC must be enabled. No cookies or encrypted audio are extracted.

Native titles use the current game track ID/configuration/localization. NetEase titles use the client's actual media session, with pending switches shown explicitly. Local tracks show filenames. Windowed/borderless mode is required for reliable add-on visibility. Natural track-end playback still follows the NetEase client's own queue; `-`/`=` follow the imported list. Local MP3/WAV playback remains available from the source menu.

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

The Add songs button opens the themed importer: paste a song or public-playlist link, preview it, then append it to the external list without replacing existing tracks. Standalone importer usage can pass `--library "<path>"`.

Configuration and list files are under `Client/Saved/Config/`; diagnostics are under `Client/Saved/Logs/PortableRadioNative.log`. The display reads an ephemeral `PortableRadioStatus-<pid>.ini` there; this local listening state is never uploaded. Public NetEase web endpoints can change and may need maintenance. Private account playlists are not supported.

To uninstall, close the game, remove this project's ASI/importer/HUD executable, and restore your backed-up loader settings. Keep your own music and configuration files unless you intend to delete them.

Original code is [MIT licensed](LICENSE). MinHook retains its original license; see [third-party notices](THIRD_PARTY_NOTICES.md). This project is not affiliated with Kuro Games, NetEase, or OptiScaler.

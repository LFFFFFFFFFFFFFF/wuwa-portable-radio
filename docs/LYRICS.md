# Song subtitle integration

The compiled LRC parser is the MIT-licensed HyPlayer/LyricParser subset pinned at
`eee1a3077ca9f1c6830eb89c64239bc788448fc1`. See
[upstream/adaptation notes](../third_party/lyricparser/UPSTREAM.md) and its
[full license](../third_party/lyricparser/LICENSE.txt). The original notice is
shipped as `LyricParser-LICENSE.txt` with built binaries. No third-party executable
or upstream build script is executed.

The existing C++ SMTC watcher reads Position, LastUpdatedTime, EndTime and playback
rate. Snapshots extrapolate while playing, stop extrapolating while paused, and
expire when the client stops reporting. A track change waits for another metadata
sample before trusting its timeline. The HUD uses a shared monotonic clock and
rejects snapshots older than four seconds. Local playback reads Media Foundation
position (`VT_I8`) and duration (`VT_UI8`) in 100 ns units. Status file writes are
limited to five per second during timed playback; a static untimed status stays
unchanged.

Only the actual matched imported NetEase track ID is sent to the public HTTPS
lyric endpoint. No login cookies, encrypted cache or audio downloads are used.
Its LRC/translation response is read with a 1 MB / 10 second bound. Availability
depends on the service; it is not a documented compatibility guarantee. Parsed
results stay in a small in-memory cache. Failed or superseded requests cannot
overwrite a newer song's subtitles. Automatic song-title search is deliberately
not used to guess unmatched tracks.

Local songs read adjacent `Song.lrc` and optional `Song.zh.lrc`. Manual imports are
validated and stored as overrides under `Client/Saved/Config/Lyrics`, keyed by
confirmed NetEase ID or a hash of the local path. Imported overrides persist;
downloaded lyrics are not bundled, published or written to disk. LRC text accepts
UTF-8 or BOM-marked UTF-16. The adapter bounds/sanitizes timestamps, applies signed
global offsets and merges duplicate timestamps around the real upstream parser.

`[Lyrics] Enabled=1`, `Translation=1`, `OffsetMs=0` are defaults. Positive offsets
show lyrics earlier. The menu persists these options in the existing Mod config.
Subtitles are click-through and only shown over the foreground game window;
windowed/borderless mode is recommended. Native game radio remains unsupported
for synchronized lyrics until its playback timeline has been verified. Existing
native audio, auto attachment and BGM suppression behavior are retained.

Validation uses synthetic lyrics, clocks, metadata and out-of-order fetches; a
read-only network probe reports only line counts. A rendered preview uses made-up
text. Actual client-to-game lyric synchronization still needs a live game test.

Primary API references: [SMTC timeline](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessiontimelineproperties),
[SMTC playback rate](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionplaybackinfo.playbackrate).

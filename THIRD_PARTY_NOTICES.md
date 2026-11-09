# Third-party notices

The root MIT license covers original project code. It does not relicense third-party code or game assets.

## MinHook v1.3.4

Source: <https://github.com/TsudaKageyu/minhook/tree/v1.3.4>

Vendored at `third_party/minhook`. Its BSD-style license, including the Hacker Disassembler Engine notices, is retained in [third_party/minhook/LICENSE.txt](third_party/minhook/LICENSE.txt) and the source headers. Include these notices with distributed binaries.

## HyPlayer LyricParser (LRC subset)

Source: <https://github.com/HyPlayer/LyricParser/tree/eee1a3077ca9f1c6830eb89c64239bc788448fc1>

Copyright (c) 2023 HyPlayer Team. MIT license, retained in
[third_party/lyricparser/LICENSE.txt](third_party/lyricparser/LICENSE.txt).
The adapted parser is compiled into RadioHud.exe; distribute its full notice as
`LyricParser-LICENSE.txt`. Compatibility adaptations and the precise source subset
are documented in [UPSTREAM.md](third_party/lyricparser/UPSTREAM.md).

## Windows and external components

The build uses the Windows SDK, Windows Media Foundation, Windows Runtime media controls, and .NET Framework. These components are provided by Microsoft and are not distributed here.

OptiScaler is a separate loader dependency; neither its source nor binaries are distributed here. The game, game assemblies, Mono runtime, NetEase client, account data, and songs are not included. Refer to the respective owners' terms for those components.

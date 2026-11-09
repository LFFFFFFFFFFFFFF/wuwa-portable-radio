# HyPlayer LyricParser subset

- Upstream: https://github.com/HyPlayer/LyricParser
- Pinned commit: `eee1a3077ca9f1c6830eb89c64239bc788448fc1`
- License: MIT, copyright (c) 2023 HyPlayer Team; full text in LICENSE.txt.
- Included: Implementation/LrcParser.cs and the four LRC abstraction types only.
- Adaptations: C# 5 / .NET Framework compatibility; string/Substring replaces Span;
  plain arrays replace ArrayPool/Array.Fill; concrete get-only properties gain
  private setters. No upstream networking, karaoke, migration or UWP code is built.
- The application validates/bounds timestamp input, applies signed global offsets,
  and sorts/merges timed lines around this parser. These changes are in src/Lyrics.cs.

The MIT notice must accompany distributed binaries incorporating this component.

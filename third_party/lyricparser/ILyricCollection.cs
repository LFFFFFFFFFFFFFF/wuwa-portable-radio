// Adapted from HyPlayer/LyricParser; see UPSTREAM.md and LICENSE.txt.
using System;
using System.Collections.Generic;

namespace LyricParser.Abstraction
{
    public interface ILyricCollection : IDisposable
    {
        IList<ILyricLine> Lines { get; }
    }
}

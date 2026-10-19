# Third-party notices

CSSE and SharedSE are derived from the supplied MWSE source archive.
Original project: https://github.com/MWSE/MWSE
The original GNU GPL version 2 license is preserved in LICENSE.txt.
Original author notices inside the source files are retained.

| Component | Files included | License / notice |
| --- | --- | --- |
| fmt | `deps/fmt/include/fmt/` | MIT-style permission notice, copyright Victor Zverovich and contributors, included in the headers and copied to `deps/fmt/LICENSE.txt` |
| toml11 | `deps/toml11/include/` | MIT license in `deps/toml11/LICENSE` |
| Microsoft DirectX 8 SDK headers | `deps/DirectX8/include/` | Microsoft copyright and notices in the original supplied headers; no DirectX binaries are included |

Dependency versions and header bytes are taken from the supplied archive;
they are not downloaded or updated during builds. The fmt license text
was copied verbatim from its supplied format.h header. The versioned Windows
SDK and MFC build components come from the developer's Visual Studio install.

The icon SVGs, PNGs and drawing geometry in scripts/generate-icons.py are
original artwork authored for this standalone update and distributed with
this GPL v2 project. They replace the loose icons from the supplied archive.
No stock icon pack or third-party icon font is bundled.
TES Construction Set, Morrowind, MGE XE, MWSE runtime files and game data
are not bundled in this repository or in the runtime package.

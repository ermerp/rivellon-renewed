# Reverse-engineering notes

Findings in our own words – **no decompiled code, no pseudocode** (CLAUDE.md rule 2). One topic per file; update the file instead of appending history. Addresses are for the verified 1.0062A build (`game-version.md`); code must find them by signature (rule 8). The column "Touches" names the game mechanics a file talks about, so a search finds every file before a feature changes one of them (conflict check, CLAUDE.md "Workflow per feature").

| File | Topic | Touches |
|---|---|---|
| [game-version.md](game-version.md) | 1.0062A hashes, PE header, German vs. English files, version history | version, language |
| [engine-map.md](engine-map.md) | Map of `div.exe`: function count and sizes, regions of `.text` (game code, built-in editor, libjpeg/libpng/zlib), subsystems by Larian's source folders with address ranges, dependencies and layers from the call graph, what it means for replacing subsystems | subsystems, command bus, built-in editor, multiplayer |
| [coverage.md](coverage.md) | Runtime share of `div.exe`: scripted runs and play sessions, share by source folder, font code | subsystems, fonts |
| [engine-classes.md](engine-classes.md) | Class hierarchies from RTTI (root classes, UI base classes `CSubWindow`/`CBitmapControl`), Larian DLL sizes | UI windows, DLLs |
| [architecture.md](architecture.md) | Imports, renderer plugins (`slash*.dll`), input/audio/video/dialog APIs, proxy and overlay decisions | renderer, input, audio, video, dialog |
| [renderer-d3d6.md](renderer-d3d6.md) | `slash1.dll` internals: how `div.exe` binds it, exports, DirectDraw/D3D6 objects, `DllSlashedGetInfo`, `CD3D6VideoMem` methods and members, `slashed-d3d6.cfg` layout | renderer, window mode, screen captures, save picture |
| [overlay.md](overlay.md) | Overlay hook point (`DllSlashedEndFrame`), drawing through `CD3D6VideoMem`, pixel formats, glyph source and font, backend interface | overlay, renderer, fonts |
| [text-rendering.md](text-rendering.md) | Which texts use `.fnt` vs. GDI (`use8bitfont`), the 61 SimSun GDI fonts, the game's GDI glyph path and pair spacing, text table (`text.cmp` in memory), text fields (Backspace), code page and Proton consequences | text, fonts, text fields, language |
| [glyph-layout.md](glyph-layout.md) | Font object/glyph/sprite layout, `.fnt` loading, drawing and line pitch (`+0x0C`), cut-off above the line top, ways to feed own glyphs, text colors (descriptors, 565 conversion, 23 original colors, stroke edges) | fonts, text colors |
| [fonts-mod.md](fonts-mod.md) | The mod's text: glyph renderer, shared style renderer (`src/text/`), `[Fonts] Colors`, short labels and menu label widths, font hook, font mode (`use8bitfont` in memory), `[Fonts] Spacing`, faces per family | fonts, text colors, menu labels |
| [gdi-raster.md](gdi-raster.md) | GDI rasterizing measured (sizing, sampling, bold, composites, metrics box, over-ascent defect) – reference for the own rasterizer | fonts |
| [config-files.md](config-files.md) | `config.div`/`config.lcl`/`slashed*.cfg`, which file decides the resolution at startup, run markers, settings object and all config keys | resolution, settings, fog, multiplayer |
| [developer-mode.md](developer-mode.md) | Developer flag (settings `+0x28`) and the forum byte patch | developer flag, built-in editor |
| [input.md](input.md) | Message loop, game window (class `Divinity`), deferred start, `WH_GETMESSAGE` hotkeys, why subclassing fails in full screen, `[Keys]` syntax | input, keys, game window, full screen |
| [key-events.md](key-events.md) | 40 key events, `divinityevent.dat`, `keylist.txt` IDs and format | keys, automap, zoom |
| [visibility.md](visibility.md) | Hero's sight range: why persons vanish on large screens, agent table and positions, distance and line-of-sight tests, the three visibility functions (hero → agent presentation, agent → agent AI), the unused `line of sight` switch, fix options | sight, clicks/targeting, AI perception, minimap, roofs/rooms |
| [movement.md](movement.md) | Hero movement by mouse: run flag and its setters, held button (walk/run circle, 3/8 of the screen height around the screen center) vs. click (pathfinding, walks), always-run field, radius per resolution, when the hero is off center, fix options incl. double click | movement, clicks, walk/run |
| [graphics-assets.md](graphics-assets.md) | 2D sprites (heroes, image lists), animation layout, 16-bit color | sprites, colors |
| [file-formats.md](file-formats.md) | Byte layouts: fonts (`ML_FONT_01`), localized texts (`text.cmp`), hero sprites, image lists; external reference Divinity_Graphics_Extender | fonts, texts, sprites, archives |
| [patch-134.md](patch-134.md) | Patch 1.34A contents, `dd-test-134` copy, 1.0034A test results (on hold) | patches, story data, savegames |
| [environment.md](environment.md) | Toolchain versions, downloads with SHA-256, dgVoodoo2 setup, Smart App Control | tools, dgVoodoo2 |

## Other sources

Knowledge bases of others about the same game. Read per question, verify in our EXE, write the part we need in own words in a topic file above; no code or pseudocode copied (rule 2), a line in `docs/credits.md` once used (rule 14).

| Source | License | Covers |
|---|---|---|
| vvoland/opendivine `re_docs/` (`github.com/vvoland/opendivine`) | GPL-3.0 | ~55 topic files and ~30 format files on the Steam 1.0062A `div.exe` (same EN hash as ours): agent struct, stats, items, combat, dialogue, Osiris, messages (command bus), inventory, object interaction, skills/magic, lighting, world clock, weather, pathfinding, GUI, quest log, savegame, shroud, eggs, treasure, objects, roofs, `.fnt`, packed sprites, `.cmp` – Ghidra-style names and pseudocode, so orientation only |
| atom0s/divproxy (`github.com/atom0s/divproxy`) | AGPL-3.0 | type layouts for 1.0062A (`libs/div/10062a/types/`): agent, agent statistics, items, objects, inventory, save manager, world, pointers |
| fstxz/divine_tools (`github.com/fstxz/divine_tools`) | GPL-3.0 | world files `world.xN` (editor), `.fnt` glyphs, `.cmp` archives (`flat`, `global`, `sound`, `voice`) |
| sormy/ddcmpa (`github.com/sormy/ddcmpa`) | MIT | `.cmp` archive layout incl. the encrypted name table |
| sormy/ddtt (`github.com/sormy/ddtt`) | MIT | text in `mono.cmp` (monologues), `itemgen.cmp` (affixes), `treasure.cmp` (item names) |
| Raan/Divinity_Graphics_Extender (`github.com/Raan/Divinity_Graphics_Extender`) | no license | sprite packs (`file-formats.md`) |
| usernameak/OsirisRE (`github.com/usernameak/OsirisRE`) | no license | Osiris 1.4 story VM |

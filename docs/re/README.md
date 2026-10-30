# Reverse-engineering notes

Findings in our own words – **no decompiled code, no pseudocode** (CLAUDE.md rule 2). One topic per file; update the file instead of appending history. Addresses are for the verified 1.0062A build (`game-version.md`); code must find them by signature (rule 8).

| File | Topic |
|---|---|
| [game-version.md](game-version.md) | 1.0062A hashes, PE header, German vs. English files, version history |
| [engine-map.md](engine-map.md) | Map of `div.exe`: function count and sizes, regions of `.text` (game code, built-in editor, libjpeg/libpng/zlib), subsystems by Larian's source folders with address ranges, dependencies and layers from the call graph, runtime share (coverage), what it means for replacing subsystems |
| [engine-classes.md](engine-classes.md) | Class hierarchies from RTTI (root classes, UI base classes `CSubWindow`/`CBitmapControl`), Larian DLL sizes |
| [architecture.md](architecture.md) | Imports, renderer plugins (`slash*.dll`), input/audio/video/dialog APIs, proxy and overlay decisions |
| [renderer-d3d6.md](renderer-d3d6.md) | `slash1.dll` internals: how `div.exe` binds it, exports, DirectDraw/D3D6 objects, `DllSlashedGetInfo`, `CD3D6VideoMem` methods and members, `slashed-d3d6.cfg` layout |
| [overlay.md](overlay.md) | Overlay hook point (`DllSlashedEndFrame`), drawing through `CD3D6VideoMem`, pixel formats, glyph source and font, backend interface |
| [text-rendering.md](text-rendering.md) | Which texts use `.fnt` vs. GDI (`use8bitfont`), the 61 SimSun GDI fonts, the game's GDI glyph path and pair spacing, text table (`text.cmp` in memory), text fields (Backspace), code page and Proton consequences |
| [glyph-layout.md](glyph-layout.md) | Font object/glyph/sprite layout, `.fnt` loading, drawing and line pitch (`+0x0C`), cut-off above the line top, ways to feed own glyphs, text colors (descriptors, 565 conversion, 23 original colors, stroke edges) |
| [fonts-mod.md](fonts-mod.md) | The mod's text: glyph renderer, shared style renderer (`src/text/`), `[Fonts] Colors`, short labels and menu label widths, font hook, font mode (`use8bitfont` in memory), `[Fonts] Spacing`, faces per family |
| [gdi-raster.md](gdi-raster.md) | GDI rasterizing measured (sizing, sampling, bold, composites, metrics box, over-ascent defect) – reference for the own rasterizer |
| [config-files.md](config-files.md) | `config.div`/`config.lcl`/`slashed*.cfg`, which file decides the resolution at startup, run markers, settings object and all config keys |
| [developer-mode.md](developer-mode.md) | Developer flag (settings `+0x28`) and the forum byte patch |
| [input.md](input.md) | Message loop, game window (class `Divinity`), deferred start, `WH_GETMESSAGE` hotkeys, why subclassing fails in full screen, `[Keys]` syntax |
| [key-events.md](key-events.md) | 40 key events, `divinityevent.dat`, `keylist.txt` IDs and format |
| [visibility.md](visibility.md) | Hero's sight range: why persons vanish on large screens, agent table and positions, distance and line-of-sight tests, the three visibility functions (hero → agent presentation, agent → agent AI), the unused `line of sight` switch, fix options |
| [movement.md](movement.md) | Hero movement by mouse: run flag and its setters, held button (walk/run circle, 3/8 of the screen height around the screen center) vs. click (pathfinding, walks), always-run field, radius per resolution, when the hero is off center, fix options incl. double click |
| [graphics-assets.md](graphics-assets.md) | 2D sprites (heroes, image lists), animation layout, 16-bit color |
| [file-formats.md](file-formats.md) | Byte layouts: fonts (`ML_FONT_01`), localized texts (`text.cmp`), hero sprites, image lists; external reference Divinity_Graphics_Extender |
| [patch-134.md](patch-134.md) | Patch 1.34A contents, `dd-test-134` copy, 1.0034A test results (on hold) |
| [environment.md](environment.md) | Toolchain versions, downloads with SHA-256, dgVoodoo2 setup, Smart App Control |

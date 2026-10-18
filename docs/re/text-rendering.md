# Text rendering (`use8bitfont 0` GDI font path)

How `div.exe` draws text with a Windows font. Bitmap font file layout (`use8bitfont 1`): `file-formats.md`. Addresses: verified 1.0062A `div.exe` (`game-version.md`).

## Path

- The font class (`CDivinityFont`, source `.\font\Font.cpp` per assert strings) has two GDI constructors: one takes face name, height, weight, italic and underline and calls `CreateFontA` (around `0x494330`); the other takes a full `LOGFONTA` and calls `CreateFontIndirectA` (around `0x493E20`). A third function (around `0x499040`) lists installed fonts with `EnumFontFamiliesExA`.
- `CreateFontA` is called with character set 0 (`ANSI_CHARSET`), quality `PROOF_QUALITY`, width 0 (aspect from the font).
- The font is selected into a **memory DC** (`CreateCompatibleDC`, based on the screen DC from `GetDC(NULL)`). Nothing is drawn with `TextOut`/`ExtTextOut`, and **no GDI call touches a DirectDraw surface** – `div.exe` does not import DirectDraw at all.
- Each character is rasterized once, on first use, with `GetGlyphOutlineA` in 8-bit gray mode (`GGO_GRAY8_BITMAP`, 65 levels). The gray levels are mapped through a 65-entry color ramp to 16-bit pixels (RGB565 or RGB555 depending on a game global that matches the renderer's 565 flag) and converted into the game's own sprite format. The result is cached per byte value (table of 256 entries); drawing is then done by the game's 2D sprite blitter into the back buffer (`renderer-d3d6.md`).
- **Which face:** the only face name in `div.exe` is `"Arial Bold"` (call around `0x5A84D1`: height 18, other parameters 0). The other fonts are built from descriptors with two face names (tried in order, then any font of the character set via `EnumFontFamiliesExA`), weight 600 and a color; the descriptors come from the GUI code (`.\BMGUI\Bmgui.cpp`), which can read an optional `fontlist.txt` (not present in the Steam installation) – where the default face names come from is open. A list of the enumerated fonts can be written to `ttfonts.lst`. Measured: up to the main menu the game creates **no** GDI font (font log, `LogLevel=debug`); they are created later in the game.
- On Windows, `CreateFontA("Arial Bold", 18, …)` gives the face **Arial** (regular weight unless a weight is passed) – there is no family named "Arial Bold" (comparison sheet, `tools/fontpreview`).
- Metrics: the space character is measured at creation (`GetGlyphOutlineA` with `GGO_METRICS`); each cached glyph keeps its black-box size, origin and advance.

## Consequences

- **Code page:** the character is passed to `GetGlyphOutlineA` as a *signed* `char`, so bytes ≥ 0x80 (umlauts, accents, Cyrillic) arrive sign-extended. How GDI interprets that value (and whether those glyphs render) is what the `use8bitfont 0` special-character test checks (`open-questions.md`). The ANSI function maps bytes through the system's ANSI code page, not the game's: Russian text (code page 1251) on a Windows-1252 system would get the wrong glyphs unless the font hook fixes that (e.g. converting with the version's code page and calling `GetGlyphOutlineW`).
- **Font hook (Phase 6 item 7):** hooking `CreateFontA` / `CreateFontIndirectA` (imports of `div.exe`) is enough to change face and size; glyph size then follows automatically. The face comes from the bundled free font, loaded for the process only (`PLAN.md` decisions); without the hook, Proton substitutes the game's Microsoft font (e.g. with Liberation), so the Deck looks different from Windows. Whether text boxes grow with it is still open (`open-questions.md`).
- **Overlay:** the game offers no GDI-on-surface path to reuse; the overlay's text approach is decided in `overlay.md`.

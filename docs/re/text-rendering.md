# Text rendering (`use8bitfont 0` GDI font path)

How `div.exe` draws text with a Windows font, and what ddmod's font hook changes. Bitmap font file layout and families (`use8bitfont 1`): `file-formats.md`. Addresses: verified 1.0062A `div.exe` (`game-version.md`).

## Which texts use which path

`use8bitfont` in `config.lcl` (or ddmod's font mode, below) switches **all** game text at once: with `1` the game opens all 61 `fonts/*.fnt` files at startup and no GDI font; with `0` it opens **no** `.fnt` file and instead creates **one GDI font per bitmap font** – the same list of 61 in the same order (fixed table in `div.exe`), every one as face **"SimSun"** (a Chinese system font – the mode was evidently made for the Chinese version), weight 600, `ANSI_CHARSET`, height per family. Measured with the font log (`LogLevel=debug`) in DE and EN, identical in both.

| Family (`file-formats.md`) | Bitmap files (`use8bitfont 1`) | GDI font (`use8bitfont 0`) | Seen in | Verified |
|---|---|---|---|---|
| Interface small caps | `font{n,gra,red,b,go,or,yel,gr}1–5` (13/13/12/10/9 px lines) | SimSun 15/14/13/12/11 | main-menu hint line (`fontn1`), version line (`fontn4`), difficulty in character creation | DE + EN, log and screenshots (menu, character creation) |
| Handwriting | `fontblack1`, `fontblack_highlight1` (12 px); `fontblack2`, `fontblack_highlight2`, `paper_green/_red` (+`_high`) (17 px) | SimSun 13; SimSun 15 | books, letters (expected) | created; in-game use pending (T1.7-W part B) |
| Menu capitals | `fontbig24` (+`highlight`, `gray`); `fontbig36` (+`highlight`, `gray`) | SimSun 24; SimSun 36 | main menu entries (`fontbig36`: normal, `highlight` = selected, `gray` = disabled) | DE + EN, log and screenshots |
| Dialog text | `dialog_white/_grey/_yellow/_red` (15 px) | SimSun 14 | dialogs (expected) | created; in-game use pending |
| Diary headings | `diary_head_1024/_800/_640` | SimSun 32/26/20 | diary (expected) | created; in-game use pending |

Call order (index 0–60, identical for `.fnt` opens and GDI fonts): `fontn1–5`, `fontgra1–5`, `fontred1–5`, `fontb1–5`, `fontgo1–5`, `fontor1–5`, `fontyel1–5`, `fontgr1–5`, `fontblack1`, `fontblack2`, `fontbig24`, `fontbig24highlight`, `fontbig24gray`, `fontbig36`, `fontbig36highlight`, `fontbig36gray`, `dialog_white`, `dialog_grey`, `dialog_yellow`, `dialog_red`, `fontblack_highlight1`, `fontblack_highlight2`, `diary_head_1024`, `diary_head_800`, `diary_head_640`, `paper_green`, `paper_green_high`, `paper_red`, `paper_red_high` – the basis for a per-family font choice in the hook.

Since no `.fnt` file is loaded with `use8bitfont 0`, every text that is drawn at all must come from these GDI fonts; part B of T1.7-W checks the in-game screens (dialogs, books, tooltips, inventory, trade, diary, map, options) and how the texts fit their boxes.

## Path

- The font class (`CDivinityFont`, source `.\font\Font.cpp` per assert strings) has two GDI constructors: one takes face name, height, weight, italic and underline and calls `CreateFontA` (around `0x494330`); the other takes a full `LOGFONTA` and calls `CreateFontIndirectA` (around `0x493E20`; the 61 game fonts come through here, return address `0x493E39`). A third function (around `0x499040`) lists installed fonts with `EnumFontFamiliesExA`.
- The font descriptors carry two face names (tried in order, then any font of the character set via `EnumFontFamiliesExA`), height, weight 600 and a color; the GUI code (`.\BMGUI\Bmgui.cpp`) can read an optional `fontlist.txt` (not present in the Steam installation). Without it the faces are "SimSun". A list of the enumerated fonts can be written to `ttfonts.lst`.
- `CreateFontA("Arial Bold", 18, …)` (around `0x5A84D1`) is the only face name in `div.exe`; it was not called up to character creation. On Windows that request gives **Arial** – there is no family named "Arial Bold".
- **Timing:** all 61 fonts are created within the first ~200 ms of the process, long before the game window exists – a hook must be in place when ddmod is loaded (the deferred start is too late).
- The font is selected into a **memory DC** (`CreateCompatibleDC`, based on the screen DC from `GetDC(NULL)`). Nothing is drawn with `TextOut`/`ExtTextOut`, and **no GDI call touches a DirectDraw surface** – `div.exe` does not import DirectDraw at all.
- Each character is rasterized once, on first use, with `GetGlyphOutlineA` in 8-bit gray mode (`GGO_GRAY8_BITMAP`, 65 levels). The gray levels are mapped through a 65-entry color ramp (the family's color) to 16-bit pixels (RGB565 or RGB555 depending on a game global that matches the renderer's 565 flag) and converted into the game's own sprite format. The result is cached per byte value (table of 256 entries); drawing is then done by the game's 2D sprite blitter into the back buffer (`renderer-d3d6.md`).
- Metrics: the space character is measured at creation (`GetGlyphOutlineA` with `GGO_METRICS`); each cached glyph keeps its black-box size, origin and advance.

## Font hook (`src/fonts/fonthook.cpp`, v0.1)

- Hooks `CreateFontIndirectA` and `CreateFontA` (gdi32 exports) in `DllMain` – the only point early enough. Calls from inside `div.exe` get the face of the font chain (`[Fonts] Chain`: Tahoma, else the bundled DejaVu Sans Condensed Bold, `overlay.md`) and the height scaled by `[Fonts] GameFontSize` (percent); weight and the game's colors stay; the charset follows `[Fonts] Charset` (Consequences). Calls from other modules are passed through. `[Features] FontHook=0` switches it off together with the font mode below.
- The chain is resolved on the first hooked call (game thread, not under the loader lock) and logged ("Font chain: using …"). Measured (DE + EN, Windows): main menu and character creation are drawn in Tahoma Bold instead of SimSun; with an unavailable first entry the bundled DejaVu Sans Condensed is loaded and used.

## Font mode (`src/fonts/fontmode.cpp`, v0.1)

`FontHook=1` also switches the game to the GDI path in memory, so `config.lcl` keeps `use8bitfont 1` (as shipped).

- **Reader:** the settings byte `+0x120` (`config-files.md`) is read in exactly one place for the choice of fonts: the font setup function (`0x45BF90`, `__thiscall`, no stack arguments; source `.\BMGUI\Bmgui.cpp`). It first tries to open `fontlist.txt`; with or without that file it reads the byte once and then either loads the 61 `.fnt` files or creates the 61 GDI fonts. The only other reader is the `config.div` writer (below).
- **Timing:** the startup function calls the font setup once (call at `0x40DBD5`), on the main thread and before the game window exists. It reads the byte through the global settings pointer (`0x0065B3D0`), so the configuration is parsed by then. ddmod is loaded earlier (ASI loader, `DllMain`).
- **Hook:** a MinHook on the font setup sets the byte to 0 before the original runs and restores the configured value afterwards; the log says "use8bitfont forced to 0 (was 1)". If the settings pointer is still null the call is passed through unchanged.
- **Signature:** the first 34 bytes of the function (`fontmode.h`), ending with `A1 <settings pointer>` and the compare of `+0x120` with 0 – unique in `div.exe`; the settings pointer address is read from the `A1` operand. Fallback RVA `0x5BF90` for the verified build only.
- **Write-back:** on exit the game writes `config.div` (`0x48C6F0`), only keys found in a set at settings `+0x154` – in practice the keys that were in `config.div` (`config-files.md`); `use8bitfont` lives in `config.lcl` and does not end up there. Because the hook restores the byte, even a `use8bitfont` line in `config.div` would be written back with the user's value.
- Measured (DE + EN, Windows): with `use8bitfont 1` in `config.lcl` and `FontHook=1` no `.fnt` file is opened, the main menu is drawn in Tahoma Bold; with `FontHook=0` the original bitmap fonts.

## Consequences

- **Code page:** the character is passed to `GetGlyphOutlineA` as a *signed* `char`, so bytes ≥ 0x80 (umlauts, accents, Cyrillic) arrive sign-extended. German umlauts and ß of the main-menu hint arrive as the correct characters (font log). The game asks for `ANSI_CHARSET` (0), so GDI maps the bytes through code page 1252. Measured in the Russian version (1.0061, `use8bitfont 0` set by hand, `game-version.md`): the 1251 bytes come out as Latin-1 letters ("ÍÎÂÀß ÈÃÐÀ"). **Setting `lfCharSet = RUSSIAN_CHARSET` (204) in the font hook is enough** – GDI then maps the same bytes through code page 1251 and the main menu shows correct Cyrillic in Tahoma Bold. The hook does this via `[Fonts] Charset` (`auto` = 1251 for the Russian EXE hash, otherwise the game's charset; `1252`/`1251` force it). The font mode does not run on 1.0061 (best effort, no RE on that EXE), so the Russian version keeps its Cyrillic bitmap fonts unless `use8bitfont 0` is set by hand.
- **Proton:** SimSun is a Windows font; under Proton the game's request is substituted by Wine – one more reason for the hook. Whether Proton's own Tahoma is found by the chain shows in the Deck log (step 7).
- **Overlay:** the game offers no GDI-on-surface path to reuse; the overlay's text approach is decided in `overlay.md`.

# Overlay backend (Phase 1 step 6)

How the mod draws its own overlay ("Rivellon Renewed <version>" + FPS, later debug and UI panels). Renderer internals: `renderer-d3d6.md`; game text path: `text-rendering.md`. Code: `src/overlay/`.

## Hook point

- MinHook on the `DllSlashedEndFrame` export of the loaded renderer DLL (`GetModuleHandle` + `GetProcAddress`, no fixed address). `div.exe` calls it through a function-pointer table from every render loop, so one hook covers game, menus and loading screens. It takes no arguments and returns a value in `eax` (cdecl); the detour passes it through.
- Draw first, then call the original: the frame's game drawing is finished, the scene is still open, `EndScene` and present follow.
- Only `slash1.dll` (Direct3D) has a backend; for `slash2`–`4.dll`/`xslash*.dll` the overlay logs a warning and stays off.
- The deferred start (`input.md`) happens while `DllSlashedInit` is still running: the export can be hooked then, but `GetInfo(3)` is null until initialization finishes → no drawing while null.
- FPS: `DllSlashedEndFrame` calls per half-second window (`QueryPerformanceCounter`), counted also while the FPS line is hidden, so it is right at once when switched on.

## What is shown

- Status line "Rivellon Renewed <version> active": `[Overlay] Status` – `banner` (default: for `BannerSeconds` = 5 s from the first drawn frame, then hidden), `always`, `off`.
- FPS line: `[Overlay] ShowFps` (default 0) sets it at start; the hotkey `[Keys] ToggleFps` (default Ctrl+Shift+F2) switches it on the game thread (atomic flag).
- When neither is shown, the detour does not lock the frame at all.
- **Save pictures:** the game reads the screen for a save's preview picture between frames (`renderer-d3d6.md`, Screen captures), so whatever the overlay shows at that moment ends up in the picture. With the banner and FPS off by default this only happens on purpose (FPS on, or saving during the banner) – a README known issue. Fix for when the overlay gets lasting content: `docs/decisions.md` (HideInCaptures).

## Drawing path: own blitter through `CD3D6VideoMem`

Per frame: `DllSlashedGetInfo(3)` → lock (vtable `+0x04`) → base pointer, pitch, width, height (`+0x10`, `+0x24`, `+0x1C`, `+0x20`) → blend into the pixels → unlock (`+0x08`). This is the mid-scene lock/write/unlock the game itself does every frame on Windows ddraw, dgVoodoo2 and WineD3D. Calls use the vtable (thiscall, no arguments).

- **Pixel format:** the lock always locks the real back buffer, also when it is not 16 bit (`renderer-d3d6.md`). So the format comes from that surface (`IDirectDrawSurface4::GetPixelFormat` on the object's `+0x24` member), looked up again whenever that pointer changes (start, resolution change). Supported: RGB565, RGB555, XRGB8888 (windowed on a 32-bit desktop without dgVoodoo2); anything else → warning, no drawing.
- **Lost surface:** the lock leaves its count unchanged when it cannot restore the surface; the backend compares the lock count (`+0x18`) before and after and only unlocks what it locked.
- **Safety:** the object layout is build-specific, so the backend only runs when `slash1.dll` has the verified SHA-256 (`game-version.md`), and it skips drawing if the object's vtable does not lie inside `slash1.dll`.
- Why not GDI text on the back buffer (`GetDC` + `DrawTextW`): the game uses `GetDC` on the back buffer only in its copy path for a non-16-bit back buffer, never with dgVoodoo2 or in full screen; GDI has no alpha; the own blitter also grows into the panel/ImGui renderer needed later (`docs/rejected.md`).

## Text

- **Glyph source** behind the `GlyphSource` interface (`glyphs.h`), so the font source can change without touching cache or renderer. Current source: our own rasterizer (`CreateFaceGlyphSource` over `text::TtFace`, 65 coverage levels → 8-bit), Unicode – rasterized once per character, the same pixels on Windows and Proton. (`CreateGdiGlyphSource` stays for the font preview tool, which compares installed faces.)
- **Font:** the font chain `[Fonts] Chain` (`src/fonts/fontchain.cpp`), shared with the game font hook (`text-rendering.md`): entries `Face @ file` are tried in order; an entry counts only if the file's own `name` table names the face (family, full or typographic family name – `text::HoldsFace`), then the file is registered for the game process (`AddFontResourceExW`, `FR_PRIVATE`, for the GDI renderer path). Installed system fonts are never used (CLAUDE.md rule 13); the log names the face used. Default: the bundled DejaVu Sans Condensed Bold from `RivellonRenewed\fonts\` (repo: `assets/fonts/`, Bitstream Vera license). Height `[Overlay] FontSize` = the em, 0 = frame height / 60 (12–48 px; 24 px at 1440p, line height 28), converted to the face's cell height like GDI does for a negative height (`text::CellHeightForEm`); a resolution change rebuilds the cache.
- Characters the font lacks are drawn as `?`. Text gets a one-pixel dark shadow; the panel behind it is black at alpha 140.
- **Option: the game's own bitmap fonts** (`fonts/*.fnt`, `file-formats.md`) – read from the installation at runtime, nothing shipped. Reader done (`CreateFntGlyphSource`, unit-tested; `dialog_white`, `fontn1`, `fontb1`, `paper_red`, `fontbig24` and `diary_head_1024` of the German copy render correctly in `tools/fontpreview`); glyphs keep their pre-shaded colors (`Glyph::colors`). Limits: fixed pixel sizes (line heights 9–38 px; at 1440p only integer scaling, which keeps the pixel look), only the characters of the installed language version (German files: no Cyrillic – the Russian version presumably ships its own), one file per color, space width estimated. Smoothed upscaling of these glyphs was tried in `tools/fontpreview` (own Scale2x and signed-distance-field scalers; xBRZ is GPL-3 and cannot be linked into the MIT-licensed mod): the distance field using the glyphs' gray edges gives clean outlines at 2× and 4× and keeps umlaut dots, Scale2x stays blocky. Not wired into `[Overlay]` yet – waits for the font decision.

## Backend interface

`Backend` (`backend.h`): `BeginFrame(SurfaceView&)` locks and describes the frame (pointer, pitch, width, height, format), `EndFrame()` unlocks. The pixel and text code (`surface.h`, `glyphs.h`) only sees the `SurfaceView` and is unit-tested without the game. Other renderers (`slash2`–`4.dll`) and a D3D11 backend (Phase 8) plug in as further `Backend` implementations.

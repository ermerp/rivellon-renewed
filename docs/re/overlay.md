# Overlay backend (Phase 1 step 6)

How ddmod draws its own overlay ("ddmod <version> active" + FPS, later debug and UI panels). Renderer internals: `renderer-d3d6.md`; game text path: `text-rendering.md`. Code: `src/overlay/`.

## Hook point

- MinHook on the `DllSlashedEndFrame` export of the loaded renderer DLL (`GetModuleHandle` + `GetProcAddress`, no fixed address). `div.exe` calls it through a function-pointer table from every render loop, so one hook covers game, menus and loading screens. It takes no arguments and returns a value in `eax` (cdecl); the detour passes it through.
- Draw first, then call the original: the frame's game drawing is finished, the scene is still open, `EndScene` and present follow.
- Only `slash1.dll` (Direct3D) has a backend; for `slash2`–`4.dll`/`xslash*.dll` the overlay logs a warning and stays off.
- The deferred start (`input.md`) happens while `DllSlashedInit` is still running: the export can be hooked then, but `GetInfo(3)` is null until initialization finishes → no drawing while null.
- FPS: `DllSlashedEndFrame` calls per half-second window (`QueryPerformanceCounter`).

## Drawing path: own blitter through `CD3D6VideoMem`

Per frame: `DllSlashedGetInfo(3)` → lock (vtable `+0x04`) → base pointer, pitch, width, height (`+0x10`, `+0x24`, `+0x1C`, `+0x20`) → blend into the pixels → unlock (`+0x08`). This is the mid-scene lock/write/unlock the game itself does every frame on Windows ddraw, dgVoodoo2 and WineD3D. Calls use the vtable (thiscall, no arguments).

- **Pixel format:** the lock always locks the real back buffer, also when it is not 16 bit (`renderer-d3d6.md`). So the format comes from that surface (`IDirectDrawSurface4::GetPixelFormat` on the object's `+0x24` member), looked up again whenever that pointer changes (start, resolution change). Supported: RGB565, RGB555, XRGB8888 (windowed on a 32-bit desktop without dgVoodoo2); anything else → warning, no drawing.
- **Lost surface:** the lock leaves its count unchanged when it cannot restore the surface; the backend compares the lock count (`+0x18`) before and after and only unlocks what it locked.
- **Safety:** the object layout is build-specific, so the backend only runs when `slash1.dll` has the verified SHA-256 (`game-version.md`), and it skips drawing if the object's vtable does not lie inside `slash1.dll`.
- Why not GDI text on the back buffer (`GetDC` + `DrawTextW`): the game uses `GetDC` on the back buffer only in its copy path for a non-16-bit back buffer, never with dgVoodoo2 or in full screen; GDI has no alpha; the own blitter also grows into the panel/ImGui renderer needed later (`PLAN.md`, rejected approaches).

## Text

- **Glyph source** behind the `GlyphSource` interface (`glyphs.h`), so the font source can change without touching cache or renderer. Current source: GDI, `GetGlyphOutlineW` with `GGO_GRAY8_BITMAP` (65 gray levels → 8-bit coverage), Unicode – rasterized once per character, like the game's own GDI font path.
- **Font:** `[Overlay] FontFace` (default Tahoma bold); optional `FontFile` is registered for the game process only (`AddFontResourceExW`, `FR_PRIVATE`) – the way to use the bundled free font once it is chosen (`open-questions.md`). Height `FontSize`, 0 = frame height / 60 (12–48 px; 24 px at 1440p); a resolution change rebuilds the cache.
- Characters the font lacks are drawn as `?`. Text gets a one-pixel dark shadow; the panel behind it is black at alpha 140.

## Backend interface

`Backend` (`backend.h`): `BeginFrame(SurfaceView&)` locks and describes the frame (pointer, pitch, width, height, format), `EndFrame()` unlocks. The pixel and text code (`surface.h`, `glyphs.h`) only sees the `SurfaceView` and is unit-tested without the game. Other renderers (`slash2`–`4.dll`) and a D3D11 backend (Phase 8) plug in as further `Backend` implementations.

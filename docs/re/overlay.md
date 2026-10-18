# Overlay backend (Phase 1 step 6)

Where and how ddmod draws its own overlay ("ddmod active" + FPS, later debug and UI panels). Renderer internals: `renderer-d3d6.md`; game text path: `text-rendering.md`.

## Hook point

- MinHook on the `DllSlashedEndFrame` export of the loaded renderer DLL (found with `GetModuleHandle` + `GetProcAddress`, no fixed address). `div.exe` calls it through a function-pointer table from every render loop, so one hook covers game, menus and loading screens.
- Draw first, then call the original: the frame's game drawing is finished, the scene is still open, `EndScene` and present follow.
- Frame access: `DllSlashedGetInfo(3)` (the `CD3D6VideoMem` back-buffer object) and `DllSlashedGetInfo(4)` (565 flag). Re-read them every frame – a resolution change recreates them.
- FPS: count `DllSlashedEndFrame` calls (or use `DllSlashedStartFrame`).
- The deferred start (`input.md`) happens while `DllSlashedInit` is still running: the export can be hooked then, but `GetInfo(3)` is null until initialization finishes → skip drawing while null.

## Text: two options

| | A: GDI on the back buffer | B: own glyph blitter through `CD3D6VideoMem` |
|---|---|---|
| How | `IDirectDrawSurface4::GetDC` on the back buffer, `DrawTextW`/`ExtTextOutW`, `ReleaseDC` | Rasterize glyphs once with GDI (`GetGlyphOutlineW`, gray levels) into a cache; per frame lock the game's `CD3D6VideoMem`, blend the glyphs into the 16-bit pixels, unlock |
| Proven by the game? | **No** – the game never calls `GetDC` on a surface; its GDI font path rasterizes into memory DCs only (`text-rendering.md`) | **Yes** – the same lock/write/unlock mid-scene that `div.exe` does every frame on all three translators |
| Back-buffer pointer | not exported: read from `CD3D6VideoMem` (`+0x24` field) or by a signature over `DllSlashedEndFrame` | `GetInfo(3)` (export) |
| Mid-scene risk | `GetDC` on the 3D render target while the scene is open is never exercised by the game; whether dgVoodoo2 and WineD3D handle it correctly (and fast) is unknown. Moving it after `EndScene` would need a hook on the translator's `Flip`/`Blt` (vtable of dgVoodoo2's or Wine's objects – against the "hook on the game side" decision) | none beyond what the game does |
| Windowed 32-bit desktop | back buffer is 32-bit, but the game draws into the 565 conversion surface → a GDI overlay on the real back buffer may be overwritten or misplaced | follows the game automatically (same object) |
| Pixel format | GDI converts | own conversion 565/555 (trivial) |
| Unicode / code pages | yes (`…W` functions) | yes (`GetGlyphOutlineW`) |
| Transparency | none (solid or transparent background only) | alpha blending possible (panel backgrounds, later ImGui draw lists) |
| Code size | small (~50 lines) | ~200 lines (glyph cache, blend, clipping) |
| Cost per frame | extra lock + GDI DIB per frame | lock is counted and the game locks anyway |

**Decision: B as the base backend** (A rejected, `PLAN.md`). It uses only what the game itself proves on Windows ddraw, dgVoodoo2 and WineD3D, so the Deck test in step 7 is unlikely to fail because of the drawing path, and it grows into the panels/ImGui renderer needed later. GDI is still used, but only to rasterize glyphs into memory – exactly like the game's own font path.
## Backend interface (planned)

`src/overlay/`: a backend gets "begin frame → surface view (pointer, pitch, width, height, format) → end frame"; the text/shape renderer draws into the view. The first backend implements it with `CD3D6VideoMem`; other renderers (`slash2`–`4.dll`) and a D3D11 backend (Phase 8) plug in later.

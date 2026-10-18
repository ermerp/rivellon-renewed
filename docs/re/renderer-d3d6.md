# Direct3D 6 renderer (`slash1.dll`)

Internals of the Direct3D renderer plugin, as far as the overlay (Phase 1 step 6) needs them. Addresses are virtual addresses at the preferred image base `0x10000000` of the verified `slash1.dll` (`game-version.md`); the DLL can be relocated, so code finds everything through exports and signatures (rule 8). The other renderers (`slash2`–`4.dll`) are not analyzed yet.

## How `div.exe` binds the renderer

- One function in `div.exe` (around `0x5576F0`) builds the file name `slash%d.dll` (or `xslash%d.dll` for an alternative set) from the renderer ID, loads it with `LoadLibraryA` and resolves all 15 `DllSlashed*` exports with `GetProcAddress` into a table of function pointers. A missing export aborts with "Can't find …".
- This happens **before** `DllSlashedInit` runs, and `DllSlashedInit` is what creates the game window. ddmod's deferred start (`input.md`) therefore comes too late to intercept `GetProcAddress`; instead it hooks the export bodies directly (MinHook on the address `GetProcAddress` returns). The exports are ordinary functions, so this works no matter who resolved them.

## Exports

| Export | RVA | Purpose |
|---|---|---|
| `DllSlashedInit` | `0x2300` | Args: window procedure, instance, "force windowed" flag. Reads `slashed-d3d6.cfg` (falls back to 800×600 full screen), enumerates DirectDraw devices, applies the configuration, retries at 640×480 on failure. Third argument non-zero → windowed. |
| `DllSlashedInternalApplyConfiguration` | `0x28D0` | Takes a 0x34-byte configuration block (same layout as `slashed-d3d6.cfg`); recreates DirectDraw, window, surfaces and device only for the parts that changed. |
| `DllSlashedStartFrame` | `0x2580` | Stores its argument, calls the texture cache's begin-frame method, then `IDirect3DDevice3::BeginScene`. |
| `DllSlashedEndFrame` | `0x25B0` | `IDirect3DDevice3::EndScene`, texture cache end-frame method (empty in this build), then presents: full screen → `Flip` of the primary (wait flag); windowed → `Blt` of the back buffer to the window's client area on the primary. On `DDERR_SURFACELOST` it restores the primary and retries once. |
| `DllSlashedGetInfo` | `0x26F0` | Returns internal objects by index (table below). |
| `DllSlashedShutdown` | `0x2460` | Writes `slashed-d3d6.cfg` (only when not forced windowed), releases everything, closes the window. |
| `DllSlashedGetResolutions`, `…GetMajorVersion`, `…GetMinorVersion`, `…GetIdentification`, `…GetDate`, `DllSlashedGlowDraw{Line,Rect,Square,Quad}` | | Not analyzed in detail. |

`DllSlashedEndFrame` starts with a 5-byte `mov eax, [device global]`, which MinHook can relocate.

## Objects and globals

DirectDraw interfaces (vtable offsets match `IDirectDraw4` / `IDirectDrawSurface4` / `IDirect3D3` / `IDirect3DDevice3`):

| Global (preferred base) | Object |
|---|---|
| `0x1005A014` | DirectDraw object (queried from `DirectDrawCreate`'s result) |
| `0x1005A018` | `IDirect3D3` |
| `0x1005A01C` | primary surface |
| `0x1005A020` | back buffer = render target |
| `0x1005A024` | `IDirect3DDevice3` |
| `0x1005A028` | viewport |
| `0x1005A02C` | game window (`HWND`) |
| `0x1004FDB8` | current configuration block (0x34 bytes) |

These globals are only documented, never used as fixed addresses: `DllSlashedEndFrame`'s code references the device, primary and back-buffer globals, so a signature over that export can read them out if needed.

`DllSlashedGetInfo(n)`:

| n | Returns |
|---|---|
| 0 | sprite container (`CSpriteContainerImp`, RTTI name present) |
| 1 | texture cache (`CTextureCacheWrapperD3D6`) |
| 2 | "initialized" flag |
| 3 | **back-buffer pixel access object (`CD3D6VideoMem`)** |
| 4 | 1 if the 16-bit format is RGB565, 0 for RGB555 |
| 5 | pointer to the configuration block |
| 6 | pointer to the device/mode lists |
| 7 | window handle |
| 8–10 | per-frame counters (reset by the texture cache at frame start) |

## Surfaces and pixel format

- **Full screen:** primary surface with one attached back buffer (flip chain, 3D-device capable). Display modes are enumerated and only **16-bit** RGB modes of at least 640 pixels width are offered.
- **Windowed:** primary surface with a clipper on the game window, plus an off-screen back buffer in video memory (3D-device capable) with the desktop's pixel format.
- **`CD3D6VideoMem`** (created on the back buffer): checks the back buffer's pixel format. If it is 16-bit RGB565 or RGB555, it gives direct access to it; otherwise (e.g. windowed on a 32-bit desktop) it creates a 565 off-screen "conversion surface". Vtable layout (byte offsets):

| Offset | Method |
|---|---|
| `+0x04` | lock (counted – nested locks only lock once; waits while the surface is busy, restores a lost surface) |
| `+0x08` | unlock (counted) |
| `+0x0C` | address of pixel (x, y) – `base + y·pitch + 2·x` |
| `+0x10` | base pointer of the locked surface (null when not locked) |
| `+0x1C` / `+0x20` | width / height |
| `+0x24` | pitch in bytes |
| `+0x28` | "direct 16-bit access" flag |

  `div.exe` caches the `GetInfo` results 0–5 in globals after every (re)configuration (around `0x557B40`; it also sets `+0x2C` of the configuration block to 1). Its frame functions – about 15 different loops (game, menus, loading screens, credits) each call `StartFrame` … `EndFrame` – lock this object, write 16-bit pixels through its base pointer, unlock it and pass it to the sprite container, all **while the Direct3D scene is open**. So locking the back buffer mid-scene is something the game itself does on Windows ddraw, dgVoodoo2 and WineD3D.

## Hook point for the overlay

- Hook `DllSlashedEndFrame` (export of whichever `slash*.dll` is loaded) and draw **before** calling the original: all game drawing for the frame is done, the scene is still open, and the next call is `EndScene` + present.
- Two ways to get pixels onto the frame, evaluated in `overlay.md`.

## `slashed-d3d6.cfg` layout (0x34 bytes)

| Offset | Meaning |
|---|---|
| `0x00` | renderer ID (0) |
| `0x04` | DirectDraw device index (-1 = default) |
| `0x08` | Direct3D device index |
| `0x0C` / `0x10` | width / height |
| `0x14` | full screen (1) / windowed (0) |
| `0x18` | gamma (float, 1.0) |
| `0x1C`–`0x24` | three sprite-container render settings |
| `0x28`–`0x30` | three device flags; `0x30` switches extra alpha/blend render states (exact meaning open) |

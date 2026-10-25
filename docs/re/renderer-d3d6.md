# Direct3D 6 renderer (`slash1.dll`)

Internals of the Direct3D renderer plugin, as far as the overlay (Phase 1 step 6) needs them. Addresses are virtual addresses at the preferred image base `0x10000000` of the verified `slash1.dll` (`game-version.md`); the DLL can be relocated, so code finds everything through exports and signatures (rule 8). The other renderers (`slash2`–`4.dll`) are not analyzed yet.

## How `div.exe` binds the renderer

- One function in `div.exe` (around `0x5576F0`) builds the file name `slash%d.dll` (or `xslash%d.dll` for an alternative set) from the renderer ID, loads it with `LoadLibraryA` and resolves all 15 `DllSlashed*` exports with `GetProcAddress` into a table of function pointers. A missing export aborts with "Can't find …".
- This happens **before** `DllSlashedInit` runs, and `DllSlashedInit` is what creates the game window. The mod's deferred start (`input.md`) therefore comes too late to intercept `GetProcAddress`; instead it hooks the export bodies directly (MinHook on the address `GetProcAddress` returns). The exports are ordinary functions, so this works no matter who resolved them.

## Exports

| Export | RVA | Purpose |
|---|---|---|
| `DllSlashedInit` | `0x2300` | Args: window procedure, instance, "force windowed" flag. Reads `slashed-d3d6.cfg` (falls back to 800×600 full screen), enumerates DirectDraw devices, applies the configuration, retries at 640×480 on failure. Third argument non-zero → windowed. |
| `DllSlashedInternalApplyConfiguration` | `0x28D0` | Takes a 0x34-byte configuration block (same layout as `slashed-d3d6.cfg`); recreates DirectDraw, window, surfaces and device only for the parts that changed. Returns 0 on failure. **Size check:** in full screen the mode setter (`0x10001E70`) accepts width/height only if the pair is in the mode list of `DllSlashedGetResolutions`, then sets exclusive cooperative level and a 16-bit display mode; windowed it takes any size (normal cooperative level, window sized to the client area). |
| `DllSlashedStartFrame` | `0x2580` | Stores its argument, calls the texture cache's begin-frame method, then `IDirect3DDevice3::BeginScene`. |
| `DllSlashedEndFrame` | `0x25B0` | `IDirect3DDevice3::EndScene`, texture cache end-frame method (empty in this build), then presents: full screen → `Flip` of the primary (wait flag); windowed → `Blt` of the back buffer to the window's client area on the primary. On `DDERR_SURFACELOST` it restores the primary and retries once. |
| `DllSlashedGetInfo` | `0x26F0` | Returns internal objects by index (table below). |
| `DllSlashedShutdown` | `0x2460` | Writes `slashed-d3d6.cfg` (only when not forced windowed), releases everything, closes the window. |
| `DllSlashedGetResolutions` | `0x2400` | One argument: a buffer for (width, height) DWORD pairs, or null. Copies the mode list and returns the number of entries (null → only the number). The list is filled while `DllSlashedInit` enumerates the display modes (16-bit RGB, at least 640 wide), so it is valid only after `DllSlashedInit`. |
| `…GetMajorVersion`, `…GetMinorVersion`, `…GetIdentification`, `…GetDate`, `DllSlashedGlowDraw{Line,Rect,Square,Quad}` | | Not analyzed in detail. |

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
- **`CD3D6VideoMem`** (0x30 bytes, constructor `0x10003690` with width, height, DirectDraw object and back buffer; vtable `0x1004327C`): checks the back buffer's pixel format. If it is 16-bit RGB565 or RGB555, it gives direct access to it; otherwise (e.g. windowed on a 32-bit desktop) it also creates a 565 off-screen "conversion surface" in system memory.

Virtual methods (thiscall, vtable byte offsets):

| Offset | Method |
|---|---|
| `+0x04` | lock, no arguments – counted (only the first call locks); **always locks the real back buffer**, also when it is not 16 bit. Retries while the surface is busy; restores a lost surface and retries once – if that fails too, the count stays unchanged and the base pointer null |
| `+0x08` | unlock (counted; the last one unlocks the surface and clears the base pointer) |
| `+0x0C` | address of pixel (x, y) – `base + y·pitch + 2·x` (only meaningful for 16-bit) |
| `+0x10` | base pointer of the locked surface (null when not locked) |
| `+0x1C` / `+0x20` | width / height |
| `+0x24` | pitch in bytes |
| `+0x28` | "direct 16-bit access" flag |
| `+0x2C`, `+0x30`, `+0x34` | copy a rectangle of 16-bit pixels between a game buffer and the frame; arguments (buffer, x, y, width, height), −1 for width/height = whole frame, the buffer's row length is the rectangle width. `+0x2C` writes the buffer into the frame, **`+0x30` reads the frame into the buffer** (the game's only way to read the screen back, see "Screen captures"), `+0x34` converts the buffer from 555 to 565 in place (only when the frame is 565) and then writes it. All end in the internal copy at `+0x38` |
| `+0x38` | internal copy, same arguments plus a direction flag (1 = buffer → frame, 0 = frame → buffer); locks the object itself. Direct access → copies row by row to/from the locked back buffer; otherwise it goes through the 565 conversion surface (`GetDC` on both + GDI `BitBlt` between back buffer and conversion surface) |

Data members (byte offsets): `+0x04`/`+0x08` width/height, `+0x0C` pitch, `+0x10` direct flag, `+0x14` base pointer, `+0x18` lock count, `+0x1C` (byte) 1 = 565, `+0x20` DirectDraw object, `+0x24` back buffer (`IDirectDrawSurface4`), `+0x28` set while the internal copy works on the conversion surface, `+0x2C` conversion surface (null with direct access).

  `div.exe` caches the `GetInfo` results 0–5 in globals after every (re)configuration (around `0x557B40`; it also sets `+0x2C` of the configuration block to 1). Its frame functions – about 15 different loops (game, menus, loading screens, credits) each call `StartFrame` … `EndFrame` – lock this object, write 16-bit pixels through its base pointer (direct access) or through the copy methods, unlock it and pass it to the sprite container, all **while the Direct3D scene is open**. So locking the back buffer mid-scene is something the game itself does on Windows ddraw, dgVoodoo2 and WineD3D; with a non-16-bit back buffer it additionally uses `GetDC` on it mid-scene.
- **dgVoodoo2 2.87.5 windowed** (2560×1440, 32-bit desktop) reports a **16-bit RGB565** back buffer – direct access, no conversion surface (T1.6-W).

## Screen captures

`div.exe` reads the screen back only through `CD3D6VideoMem` `+0x30`, always for the whole frame (x = y = 0, width = height = −1), between lock (`+0x04`) and unlock (`+0x08`) on the object it caches from `GetInfo(3)`. Four readers:

| Reader (1.0062A) | Purpose |
|---|---|
| savegame shot (`0x4A9350`, file name as argument) | writes the frame to `dynamic\shot.xxx`: uncompressed 24-bit TGA (18-byte header, bottom-up rows), 565 or 555 depending on the settings. Followed by a thumbnail function (`0x4A9690`) that reads the frame again via `+0x30` and box-filters it into a small 16-bit image inside a game object (`+0xCC` of the object at global `0x741F24`). The save copies `dynamic\shot.xxx` into `savegames\<name>\shot.xxx`; the load screen shows it |
| bug report (`0x518ED0`, `BugReportPlate.cpp`) | frame as 24-bit RGB buffer |
| editor map capture (`0x5A5D99`) | frame scaled down tile by tile into `capture\big_shot.png` (developer feature) |

The savegame shot is taken from three places: a key handler for key code `0x1B` (Escape – before the game menu opens, so this is presumably the picture of a normal save), the quicksave path in `GAME\render.cpp` (`0x4A9FB0`, `savegames\quicksave.<n>`), and one Osiris story command. A capture outside the drawing of a frame reads the last finished frame – including everything drawn at `DllSlashedEndFrame`, which is how the overlay gets into saves. Which captures run between `EndFrame` and the next `StartFrame` and which inside a frame is open (runtime check).

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

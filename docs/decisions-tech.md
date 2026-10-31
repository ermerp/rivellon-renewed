# Decisions – plugin and engine work

One line per decision with its reason; part of [decisions.md](decisions.md) (project, direction, platforms), text and fonts: [decisions-fonts.md](decisions-fonts.md). Rejected approaches: [rejected.md](rejected.md).

## Plugin core

- **MinHook v1.3.4 via FetchContent (pinned commit), own CMake target** – same static CRT/x86 settings as the plugin.
- **Own SHA-256 instead of bcrypt** – hashing runs in `DllMain`, where loading `bcrypt.dll` under the loader lock is unsafe.
- **Deferred start via a watcher thread** – hooks go in once the window of class `Divinity` exists (loader lock; the renderer DLL is not loaded before).
- **Log history:** previous start as `RivellonRenewed.prev.log`, older ones in `RivellonRenewed\logs\` by start time (`[General] LogHistory`, default 20) – testers restart often and report by screenshot time; the start time comes from the log's first line (NTFS keeps old creation times).

## Input and movement

- **Hotkeys via a `WH_GETMESSAGE` hook on the game thread** – independent of who owns the window procedure (dgVoodoo2, WineD3D); swallowed keys become `WM_NULL`.
- **No F10 and no Alt combinations as default hotkeys** – a missed key puts the game into Windows' menu mode.
- **F10 and a lone Alt reach the game as normal keys (`[Features] BlockMenuKey=1`)** – they put a framed window into Windows' menu loop (the "F10 softlock"); rewriting keeps the key for the game.
- **Own key repeat for Backspace, Delete, Left, Right in text fields (`[Features] KeyRepeat=1`)** – human's wish; the text fields act on key-up, so the hook posts one key-up at once and more from a timer (no memory access); `[Input] RepeatDelay=300` ms, `RepeatRate=15`/s – Windows' 500 ms / 30/s felt wrong.
- **Walk/run circle as a fixed pixel radius, default 225 px at every resolution (`[Movement] WalkRadius`, `original` = 3/8 of the screen height)** – the world is drawn 1:1, so pixels mean the same world distance everywhere; value from the human's feel test (T1.19-W); set in the INI only.
- **Double click = run for click moves (`[Movement] DoubleClickRun`, default on)** – a single click always walks in the original.
- **The hero stops when a held button is released (as in the original)** – the human finds it right.

## Window and graphics

- **Borderless full screen through dgVoodoo2's `FullscreenAttributes = fake`, set by the mod, default on (`[DgVoodoo] FullScreen=borderless`)** – the human plays full screen and wants Alt+Tab; dgVoodoo already does it, no window code of our own.
- **dgVoodoo's watermark switched by the mod (`[DgVoodoo] Watermark`, default off)** – rewriting one key in `dgVoodoo.conf` at every start is simpler than intercepting its config reads; `keep` leaves the file alone.
- **Windows full-screen optimizations: README note only, no compatibility flag** – the reported crashes did not show in nine runs (T1.20-W); a registry change by the installer is more invasive.
- **Fog on by default, the INI is the master (`[Features] Fog=1|0|keep`)** – Steam's `config.div` switches the ground fog off with `no fog`; the game's performance page writes its choice back into the INI.
- **Overlay base backend: DirectDraw/D3D6 at `DllSlashedEndFrame`** – the only path that works with Windows ddraw, dgVoodoo2 and WineD3D.
- **Overlay text: own glyph blitter through the game's `CD3D6VideoMem`** (`DllSlashedGetInfo(3)`) – repeats the mid-scene lock/write/unlock the game does every frame, so the path is proven on all platforms and pixel formats and allows alpha blending.
- **Overlay for v0.01: start banner for ~5 s, FPS off by default** (`[Overlay] Status=banner|always|off`, `ShowFps`, `[Keys] ToggleFps` Ctrl+Shift+F2) – the game reads the screen for a save's preview picture, so a permanent overlay ends up in every save. README known issue: FPS can appear in save pictures.
- **Start banner in the game's look, top left at fixed pixel sizes (`[Overlay] BannerLook=game`)** – "RIVELLON RENEWED" in the menu family at `fontbig24`'s size, "v0.01" in the load list's white (`fontn2`), "ACTIVE" in the load title's green (`fontgr1`), pen x 18, baseline y 38; capitals because Uncial Antiqua's lowercase misreads. Human's choice (variant 1, `t1.17\03`).
- **Own option arrows: plain triangles in the original's box and measured style (variant A)** – human's choice from five shapes; smooth like the original, no menu gloss.
- **Workaround `[Fonts] ShortLabels` (DE "HINTERGRUNDGERÄUSCHE" → "HINTERGRUND")** – the only label over an arrow.

## Testing

- **Automated walkthroughs by real input (`Start-GameTest.ps1 -Steps`) with known sequences** – only when the human has said the PC is free.
- **`Start-GameTest.ps1` runs in full screen by default (`-Windowed` for the old way), all-black shots are kept and marked `-BLACK`** – players run borderless full screen, and a real black screen must show up in the results instead of being skipped.
- **`Start-GameTest.ps1 -Intro`** – the EN copy ships `startup logo 0`; DE and EN must run under the same conditions.
- **Resolution tests set `config.div` and offsets 0x0C/0x10 of `slashed-d3d6.cfg` directly** instead of running `configtool.exe` – scriptable, same effect.

# Decisions

One line per decision with its reason; the human keeps these decisions (CLAUDE.md). Rejected approaches: [rejected.md](rejected.md). Full wording before the cleanup: [archive/plan-2026-10.md](archive/plan-2026-10.md).

## Project and release

- **Name "Rivellon Renewed", file stem `RivellonRenewed`** (`.asi`, `.ini`, `.log`, `.prev.log` in the game folder, everything else in `RivellonRenewed\`) – the full name stands out among hundreds of game files; one folder keeps the game folder tidy. Internal identifiers (`namespace ddmod`, CMake targets, `Install-DdMod`) and `C:\dev\dd-modern` stay. No Larian logo; README disclaimer "Not affiliated with or endorsed by Larian Studios".
- **First release v0.01** (`MOD_VERSION` `0.01` in `CMakeLists.txt`; banner "v0.01", zip `RivellonRenewed-0.01.zip`).
- **MIT license for our code, copyright holder `ermerp`** – usual for mods, fits every bundled component (OFL/DejaVu ship with their license files, MinHook BSD-2 needs its notice); the pseudonym keeps the human's name out of the repo.
- **Commits under the GitHub noreply address**; user paths and LAN IPs stay out of the repo (Deck IP in Claude's local memory).
- **Branches: one per feature or fix, merged into `main` with a merge commit after its test, then deleted; a release is a tag on `main`** – simple for one person; `main` may be ahead of the latest release (`CHANGELOG.md` "Unreleased"), users install from Releases. Pushed branches may stay visible on GitHub (hiding them is not worth it).
- **GitHub page like `ermerp/masterthesis`:** badges latest tag, release date, commits since latest release; releases in the sidebar; short README, details as Markdown sub-pages in `docs/` (GitHub Pages later – it costs money while the repo is private).
- **Every release gets a final run with the release build** – the human's release test plus one start of each earlier automated check (e.g. one new-game start for T1.21-W), not the full matrix.
- **Sight-range fix in v0.02, not v0.01** – human's choice.

## Direction

- **Original look, modernized gameplay; after v0.01 gameplay comfort before graphics/HD** – the order of gameplay features comes from the human's play phase (`docs/playtest-log.md`), not from guesses.
- **The whole game in 16:9 and 16:10, filling the screen** – the Deck is 1280×800 (16:10), so layouts, upscaling and HD work must handle both; 4:3 is secondary. The Deck runs the game at 1280×800, not 1280×720 with bars.
- **Two presets ("Rivellon Renewed recommended", "technical + fixes only"), exploits kept by default** – the base game stays as players know it; every feature and exploit fix individually switchable.
- **Course after v0.01: replace subsystems step by step, no full rewrite** – each feature a module with its own logic behind a thin hook layer, only as deep into the engine as the feature needs; the game stays playable at every step and our functions can be compared with the original in-process. A full engine rebuild (roadmap Phase 11) stays an outlook.
- **Replacements are clean-room, fonts first** – understand with the decompiler (scratchpad only), document in own words, own implementation, comparison test, switchable hook (rule 2). Fonts first (~90 functions, 22 entry points, pixel-exact comparison, no game state; `docs/re/engine-map.md`), then the UI through `CSubWindow`/`CBitmapControl`; the gameplay core only by targeted changes.
- **No system dependencies (rule 13)** – bundled resources and own code, so one build looks the same on Windows and Proton; unavoidable system parts behind a thin hook layer.

## Platforms and versions

- **Target EXE 1.0062A** (Steam/GOG) – everyone has it, handles 2560×1440; 1.0034A needs a CD-path workaround (`docs/re/patch-134.md`).
- **Language priority: 1. EN + DE, 2. FR, 3. RU** – EN/DE fully tested; FR same build, spot test per release (accents); RU (own EXE 1.0061) best effort: overlay and input run, original Cyrillic bitmap fonts, `[Fonts] Charset=auto` picks 1251 by EXE hash. Full RU support is open after v0.01.
- **Proxy: Ultimate ASI Loader as `winmm.dll`** – `div.exe` imports it; `ddraw.dll`/`D3DImm.dll` belong to dgVoodoo2; coexists with OsirisRE's `osi_extender`.
- **dgVoodoo2 on Windows only** – it crashes under Proton (T0.6-D).
- **Every phase tested in `dd-test` (DE) and `dd-test-en` (EN)** – same `div.exe`, different language files.
- **Separate play copy `C:\dev\dd-play` with permanent coverage monitoring** – the human's real saves stay apart from test saves and automated runs; every session adds runtime data.

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
- **Overlay base backend: DirectDraw/D3D6 at `DllSlashedEndFrame`** – the only path that works with Windows ddraw, dgVoodoo2 and WineD3D; D3D11 via dgVoodoo2 only optional for Phase 8.
- **Overlay text: own glyph blitter through the game's `CD3D6VideoMem`** (`DllSlashedGetInfo(3)`) – repeats the mid-scene lock/write/unlock the game does every frame, so the path is proven on all platforms and pixel formats and allows alpha blending.
- **Overlay for v0.01: start banner for ~5 s, FPS off by default** (`[Overlay] Status=banner|always|off`, `ShowFps`, `[Keys] ToggleFps` Ctrl+Shift+F2) – the game reads the screen for a save's preview picture, so a permanent overlay ends up in every save. README known issue: FPS can appear in save pictures.
- **Planned: `[Overlay] HideInCaptures` once the overlay shows lasting content** – hook `CD3D6VideoMem` `+0x30` (the game's only screen read) plus `DllSlashedStartFrame`; a read outside a frame gets the pixels under the panel written into the game's copy (`docs/re/renderer-d3d6.md`).
- **Start banner in the game's look, top left at fixed pixel sizes (`[Overlay] BannerLook=game`)** – "RIVELLON RENEWED" in the menu family at `fontbig24`'s size, "v0.01" in the load list's white (`fontn2`), "ACTIVE" in the load title's green (`fontgr1`), pen x 18, baseline y 38; capitals because Uncial Antiqua's lowercase misreads. Human's choice (variant 1, `t1.17\03`).
- **Own option arrows: plain triangles in the original's box and measured style (variant A)** – human's choice from five shapes; smooth like the original, no menu gloss.
- **Workaround `[Fonts] ShortLabels` (DE "HINTERGRUNDGERÄUSCHE" → "HINTERGRUND")** – the only label over an arrow; back to the original texts when the menus are redesigned, then remove `src/features/shortlabels.*`.

## Text and fonts

- **Text only from vector fonts, no upscaled original glyphs** – scales to any size and is the first step to own material.
- **Target architecture: one text renderer of our own** (vector glyph plus a style per family: color profile, rim, gloss, texture), shared by game texts, overlay and later own UI. From the originals only measured style parameters, no pixels; effects only where the original has them. Prototype `tools/fontpreview/scene.cpp`.
- **Game texts through own glyph tables in the bitmap path** – each `.fnt` font's glyphs replaced after loading: per-pixel color, per-family faces, own line height (the game re-paginates), no GDI in game; proven by T1.13-W.
- **v0.01 ships the glyph-table renderer, the GDI path stays as fallback** (`[Fonts] Renderer=glyphs|gdi`).
- **Faces per family (OFL, bundled):** menu **Uncial Antiqua** (weight 600, gloss variant 5 of `candidates6\Menue-Glanz.pdf`), diary title **Almendra SC** (measured style), interface **Marcellus SC** (600), books/letters **Fondamento** (400) – at the original's capital height; human's choice from five rounds of sheets. Books: at 12 px pitch neighboring lines touch – accepted; `[Family.Books] LineHeight=auto` widens it.
- **Dialog and overlay: the bundled DejaVu Sans Condensed Bold through the own rasterizer on every platform; Tahoma removed** – Tahoma is not shippable and Wine's look-alike is thinner (T1.26-D); DejaVu is pixel-identical on Windows and the Deck and has Cyrillic. The font chain takes only `Face @ file` entries whose file names that face.
- **Own rasterizing of the bundled faces: stb_truetype plus own emboldening (plan B), not GDI (`[Fonts] Raster=own`)** – Wine's synthetic bold makes the faces narrower and thinner (T1.24-D); our code repeats GDI's measured sizing, sampling and overstrike bold, so the accepted Windows looks stay (`docs/re/text-rendering.md`).
- **Menu and diary title matched on all capitals (median over A–Z), not on the H** – Almendra SC's and Uncial Antiqua's H stand 5–6 px above their other capitals (T1.17-W). Interface, books and dialog stay on the H.
- **Ink above the line top: accents move close, then the whole font lower (`[Family.*] TopFit=lower`, menu and diary title)** – the game cuts off everything above the line top there; menu 8 px lower (the whole D), diary title 6 px lower. Human's choice over `fit` (kept as an option), `squeeze`, `none`.
- **Symbol slots get own vector symbols** – the menu fonts hold the ▶/◀ arrows in `[`/`]` (no other font has symbols); `keepOriginal` stays for `Arrows=original`.
- **Russian edition keeps its Cyrillic bitmap glyphs** – the four OFL faces have no Cyrillic; DE/FR are fully covered, a missing single character keeps the original glyph.
- **GDI path (`Renderer=gdi`):** `use8bitfont 0` set in memory by the font hook, not in `config.lcl` (no file change, same on the Deck); control characters get no glyph (Backspace works); `[Fonts] Spacing=font` by default (the game's touch test merges periods); `[Fonts] Colors=original` from a fixed table of the measured bitmap colors, applied only while the game's calls match it.

## Testing

- **Automated walkthroughs by real input (`Start-GameTest.ps1 -Steps`) with known sequences** – only when the human has said the PC is free.
- **`Start-GameTest.ps1 -Intro`** – the EN copy ships `startup logo 0`; DE and EN must run under the same conditions.
- **Resolution tests set `config.div` and offsets 0x0C/0x10 of `slashed-d3d6.cfg` directly** instead of running `configtool.exe` – scriptable, same effect.

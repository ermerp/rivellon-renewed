# Test results

One line per finished test, newest first. `-W` = Windows, `-D` = Steam Deck. Full protocols up to T1.27: [archive/testing-2026-10.md](archive/testing-2026-10.md) (search for the ID).

| ID | What | Where | Result | Date |
|---|---|---|---|---|
| T1.36-W | Release zip with the MinHook license (automated) | `build\dist`, commit `7b64120` | passed: `RivellonRenewed/licenses/MinHook.txt` = the pinned checkout's `LICENSE.txt` (read back), build and unit tests pass | 2026-10-30 |
| T1.35-W | Reproducible build (automated): two builds in different folders, two release script runs from commit `d496459` | scratchpad, `build\dist`, `dd-test-first` | passed: without `/Brepro` the `.asi` differed in 4 bytes (PE time stamp), with it byte-identical; both zips SHA-256 `4c57a04d…03d6`; start test from the zip as in T1.34-W | 2026-10-30 |
| T1.34-W | Release script (automated, no real input): zip from `HEAD` `0739b43`, unpacked into a never-started copy, setup install, start test, setup uninstall | `dd-test-first` (fresh copy of `dd-steam-clean`) | passed: both loader downloads match their pinned SHA-256, unit tests pass, zip read back equals its sources; menu with banner and all five faces at 1920×1080 (`auto`); after uninstall only the files the game writes itself differ (as T1.32-W) | 2026-10-30 |
| T1.32-W | Setup by double click (human): dgVoodoo2 install, game start, uninstall | `dd-test-fresh` (never-started copy of `dd-steam-clean` + mod, no dgVoodoo2) | passed: no problems, no Windows warning reported; afterwards no mod or dgVoodoo2 file left, only the files the game writes itself differ from the fresh install (`dynamic\`, `savegames\`, `init.cfg`, `sinfo.000`, `keylist.txt`, `mapids.000`, `persist.dat`, `slashed-d3d6.cfg`) | 2026-10-30 |
| T1.33-W | Black screen at the very first new game of a fresh install (human, real input) | `dd-test-first` (never-started copy of `dd-steam-clean` + dgVoodoo2 borderless) | not reproduced: world visible in A (no mod, 1024×768), B (mod, `FullScreenMovies=1`, 1920×1080), C (mod, `FullScreenMovies=0`, intro skipped), D (exact rebuild of `dd-test-fresh` before the sighting: same build, INI, dgVoodoo from the setup); B: desktop with a black box between Larian logo and intro (known mode-switch flash), C: none; after A the primary monitor stayed at 800×600 (once, registry mode unchanged) | 2026-10-30 |
| T1.31-W | Setup script: dgVoodoo2 download/install, second run, game start with dgVoodoo's default conf, uninstall (automated) | `dd-test-fresh` (copy of a fresh Steam install) | passed: progress in 10 % steps, SHA-256 OK, second run skips; game at 1920×1080 RGB565 through dgVoodoo; after uninstall only the game's own `dynamic\` files differ from the fresh install, `.cmd` gone | 2026-10-25 |
| T1.30-D | Resolution `auto` and full-screen movies on the Deck | Deck (DE), Game Mode | passed: Proton reports 1280×800, `auto` picks it (before 1280×720), in `slash1.dll`'s list (17 modes up to 1280×800); menu at 1280×800; startup and menu intro switch to 640×480, gamescope scales to full height with side bars (human); 0 warnings | 2026-10-25 |
| T1.29-W | Full-screen movies (`[Display] FullScreenMovies=1`), dgVoodoo scaling aspect, automated (no input) | DE, 2560×1440 windowed and borderless full screen | passed: `larian.bik` at 640×480 scaled to full height with 4:3 side bars, back to 2560×1440 after the movies, banner only afterwards; `config.div` unchanged | 2026-10-25 |
| T1.28-W | Resolution picked by the mod (`[Display] Resolution=auto`), borderless full screen | DE (limit 1080 → 1920×1080), EN (no limit → 2560×1440) | passed: fills the screen, mouse exact, Alt+Tab fine, 1080 scaled up slightly soft (human: acceptable); `config.div` unchanged. Main menu (1024×768) and intro movie (640×480) stay centered as in the original | 2026-10-25 |
| T1.27-W/D | Dialog and overlay in the bundled DejaVu Sans Condensed Bold, no installed fonts | DE, EN, Deck | passed, 0 warnings; dialog pixel-identical on Windows and the Deck | 2026-10-25 |
| T1.26-D | Plan B (own rasterizer) on the Deck after a clean install | Deck 1280×720 | passed; banner 412×39 px as on Windows (T1.24-D: 378); human's choice DejaVu for the dialog, accepted in their own Deck play test | 2026-10-25 |
| T1.25-W | Own rasterizer `[Fonts] Raster=own` vs. `gdi` (A: menu, B: in game) | DE, EN, FR | passed, 18 runs without warning; only change: menu 8 px lower instead of 7 (whole D, human's choice) | 2026-10-24/25 |
| T1.24-D | Phase 1 on the Deck (Proton, no dgVoodoo), driven over SSH | Deck 1280×720 | passed (log, hooks, overlay, keys, walk/run, save/load); faces narrower under Wine's GDI bold → plan B | 2026-10-24 |
| T1.23-W | Own key repeat for Backspace, Delete, Left, Right (`[Features] KeyRepeat`) | DE, EN | passed, 300 ms then every 67 ms; accepted by the human in full screen | 2026-10-24 |
| T1.22-W | Ctrl+Shift+F2 in game, potion on F2, held Backspace | DE, EN | passed; held Backspace deletes one character also without the mod (original behavior → T1.23) | 2026-10-24 |
| T1.21-W | Black screen at the start of a new game | DE, EN | not reproduced in 18 of 18 starts (mod, `Fog=0`, no mod); one new-game start per release | 2026-10-24 |
| T1.20-W | Borderless full screen, F10/Alt menu key (`[DgVoodoo] FullScreen`, `[Features] BlockMenuKey`) | DE, EN | passed, no crash in nine runs; feel test accepted (Alt+Tab back in about a second) | 2026-10-24 |
| T1.19-W | Walk radius and double click run (`[Movement]`) | DE, EN | passed; the human settled on 225 px for every resolution | 2026-10-24 |
| TRE.1-W | Play sessions with function coverage in `dd-play` (by the human) | DE | three sessions; game code 55 % of functions (58 % of bytes); ranged weapon, spells, level-up not yet played | 2026-10-23 |
| T1.18-W | Own option arrows, "v0.01" banner, fog switch, short labels | DE, EN, FR | passed; "HINTERGRUNDGERÄUSCHE" the only label over an arrow → `ShortLabels` | 2026-10-23 |
| T1.17-W | Capitals matched on all letters, banner in the game's look, `TopFit` | DE, EN, FR | passed after a fix (accent past the mask); human's choice `lower` for menu and diary title | 2026-10-23 |
| T1.16-W | Glyph renderer on the remaining screens (R4) | DE, EN at 1600×900 | passed after a fix (menu arrows kept in the `[`/`]` slots); accepted by the human | 2026-10-23 |
| T1.15-W | Language spot test FR/RU, dialog face's file (R3) | DE, EN, FR, RU at 1600×900 | passed | 2026-10-23 |
| T1.14-W | Glyph renderer with faces per family (R2) | DE, EN | passed, accepted; change request diary title (done in T1.17) | 2026-10-23 |
| T1.13-W | Own glyph tables, proof of concept | DE, EN | passed; the game's page break follows a wider line pitch | 2026-10-22 |
| T1.12-W | Overlay start banner and FPS toggle | DE, EN | passed (A–C) | 2026-10-21 |
| T1.11-W | dgVoodoo watermark switch | DE, EN | passed | 2026-10-20 |
| T1.10-W | Rename to Rivellon Renewed, migration of the working copies | DE (all four migrated) | passed | 2026-10-20 |
| T1.9-W | Original text colors in the font mode (`[Fonts] Colors`) | DE, EN | passed after a fix (565 rounding), accepted from the sheets | 2026-10-20 |
| T1.8-W | Language check FR/RU | FR, RU | passed; RU best effort (original fonts) | 2026-10-19 |
| T1.7-W | Game texts through GDI and the font hook | DE, EN | A/A2/C passed; B (human) found three bugs, fixed in C; D: `Spacing=font` | 2026-10-18–20 |
| T1.6-W | Minimal overlay | DE, EN | passed (A–C); back buffer RGB565 also windowed | 2026-10-18–20 |
| T1.5-W | Input hook and test hotkey | DE, EN | passed with `WH_GETMESSAGE`; the window-procedure subclass failed in full screen | 2026-10-18 |
| T2.0-W | Debug key events and config options (human) | EN | part A not testable (`keylist.txt` is only an export), part B answered | 2026-10-18 |
| T1.4-W | EXE hash check, scanner, hook manager | DE, EN | passed | 2026-10-18 |
| T1.1-W | ASI loader and plugin load | DE | passed | 2026-10-18 |
| T0.6-W | dgVoodoo2 with 1.0062A | DE | passed (1920×1080, 2560×1440) | 2026-10-18 |
| T0.6-D | dgVoodoo2 under Proton | Deck | failed (crash in dgVoodoo's `DDraw.dll`) → no dgVoodoo on the Deck | 2026-10-18 |
| T0.7-W | 1.0034A EXE | `dd-test-134` | open, on hold (`docs/re/patch-134.md`) | – |

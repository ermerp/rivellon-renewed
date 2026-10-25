# Test protocols until v0.01 (archive)

Full protocols of the tests T0.6 to T1.27 as they stood in `docs/testing.md` before the cleanup. Not maintained and not read at session start – search it when a detail of an old test is needed. One-line results: [test-results.md](../test-results.md); current test procedures: [testing.md](../testing.md).

## T1.26-D – Plan B on the Steam Deck after a clean install (Proton; Claude over SSH, Game Mode)

Goal: the bundled faces look the same on the Deck as on Windows with `[Fonts] Raster=own` (T1.24-D: narrower and thinner through Wine's GDI bold), and the dialog decision Wine Tahoma (GDI) vs. DejaVu Sans Condensed Bold (own rasterizer) (`PLAN.md`, Decisions).
**Preparation:** the human's clean install (never started); `deck.sh check`, `deck.sh install --loglevel debug` (backup `_backup_rr_<time>`), `deck.sh testsaves T-Brief-Buch-Inventar T-Dialog-links T-Nebel T-draussen T-Trank-F2` (creates `savegames\` on a fresh install); `config.div` and `slashed-d3d6.cfg` set to 1280×720 (fresh install: 1024×768 / 800×600); launch options set by the human.
**First start of a fresh install:** Steam runs the game's install script and then opens `configtool.exe` ("Slashed Configuration v1.2", on display `:1`) – it showed Direct 3D and 1280×720 already; "Apply & Close" deletes `slashed-d3d6.cfg`, the game writes it again (1280×720) on exit. That first `div.exe` ran **without the launch options** (no `winmm=n,b`, no mod log); after quitting it from the menu, Steam started the game again with them. `xdotool mousemove --window` does not move the pointer over `configtool` – `mousemove_relative` does.
**Steps:** banner, main menu, options → Video → picture settings (arrows), load dialog, `T-Brief-Buch-Inventar` (status T, inventory I → chest tab `0.673 0.6` → letter `0.4945 0.6736`, close `0.399 0.686` → book 1 `0.538 0.671`), diary Q (close `0.770 0.851`), `T-Dialog-links` (load list row 5 `0.345 0.371`, then `click 0.02 0.5`), quit via the menu (Beenden `0.5 0.840`, ✓ `0.476 0.521`); then once more with `[Family.Dialog] Face=DejaVu Sans Condensed @ …` (dialog only). The same screens on Windows (`dd-test`, windowed 1280×720, own). The inventory, letter and book open at places stored in the save – the T1.24-D click points do not fit this save state. Deck shots lag one step behind sometimes – wait 3–4 s before a `shot`.
**Result (2026-10-25, SteamOS Game Mode, Proton 10.0, game 1280×720 RGB565):** passed. Log: "Platform: Wine/Proton", EXE hash verified, 0 warnings/errors in all starts; Interface, Books, Menu, DiaryTitle "own rasterizer", Dialog "GDI rasterizer" (Proton's Tahoma Bold, `C:\windows\Fonts\tahomabd.ttf`), 61 of 61 fonts, 7249 glyphs. **Banner 412×39 px on the Deck as on Windows** (T1.24-D: 378). Side by side with Windows (`C:\dev\screenshots\deck\t126\cmp\`, plus the unmodded Deck menu and quit dialog from the first start): banner, main menu, picture settings with own arrows, load dialog, status window, letter, book, diary and quit dialog match – same widths, weights, line breaks and colors. Diary title "AUFTRÄGE" at 1280×720 sits on the parchment's top edge – same on both platforms (layout at this resolution, not a Deck issue). **Dialog:** Wine's Tahoma Bold through GDI is clearly thinner and wider spaced than Windows' Tahoma Bold (reads like a regular cut); DejaVu Sans Condensed Bold through the own rasterizer is close to the Windows look (a little wider). Comparison with the original at 1280×720 (`cmp\11-dialog-faces.png`): DejaVu on Windows and on the Deck pixel-identical (0 of 4211 text pixels differ), Wine's Tahoma over 4000. **Human's choice: DejaVu for every platform, Tahoma removed** (T1.27). The human's own play test on the Deck with the T1.27 build: accepted (log without warnings).

## T1.27-W/D – Dialog and overlay in DejaVu Sans Condensed Bold, no installed fonts (rule 13)

**Change:** `[Family.Dialog]` and `[Fonts] Chain` default to the bundled DejaVu Sans Condensed Bold; a chain entry needs a file whose `name` table names the face (installed fonts and the registry lookup are gone); the overlay's FPS/plain line uses the own rasterizer. `Install-DdMod` drops a leading `Tahoma,` from taken-over `Chain`/`Face` values.
**Preparation:** build, `ctest` (3446 checks: no entry without a file, a file without the named face is refused, every default face is a bundled file, DejaVu with Cyrillic, overlay glyphs from the own rasterizer); `Install-DdMod` in `dd-test`/`-en` (`LogLevel=debug`), `deck.sh install --loglevel debug`.
**Steps:** banner, FPS on/off (Ctrl+Shift+F2), NPC dialog (`T-Dialog-links`) – DE + EN windowed 2560×1440, Deck 1280×720.
**Result (2026-10-25):** passed, 0 warnings. Log: "Font chain: using \"DejaVu Sans Condensed\" (file …)", "family Dialog - \"DejaVu Sans Condensed\" weight 700 … own rasterizer", "Overlay font: DejaVuSansCondensed-Bold.ttf 24 px (line height 28), own rasterizer" (Deck 12 px); taken-over INIs of `dd-test`/`-en` lost their `Tahoma,` entry. Screens `C:\dev\screenshots\t127\`, `C:\dev\screenshots\deck\t127\`.

## T1.25-W – Own rasterizer for the bundled faces (`[Fonts] Raster`, plan B)

**Preparation:** build, `ctest` (`TestTtFace`: at every height the glyph renderer uses, own rasterizer and GDI give the same height choice, ascent, cell height, capital metrics, books' ink extent and advances, shapes within 3 %, bold = overstrike); `Install-DdMod` in `dd-test`/`-en`, `LogLevel=debug`.
**Part A (main menu, no input):** `Start-GameTest.ps1 -NoIntro -Seconds 26 -Count 1`, DE and EN, once with `[Fonts] Raster=own` and once with `gdi`.
**Part B (in game, real input, only when the PC is free):** the T1.14-W/T1.16-W sequences (load dialog, status window, diary, letter, book, NPC dialog, options page with arrows) with `Raster=own` and `gdi`; FR spot test in `dd-test-fr` (accented capitals).
**Expected result:** log "Fonts: family … own rasterizer" for Interface, Books, Menu, DiaryTitle and "GDI rasterizer" for Dialog; the same glyph-renderer line per font as with `gdi` (height, capital height, line height); texts and banner look like the accepted T1.14-W–T1.18-W screens, at most single gray levels differ; accented capitals of Marcellus SC (French) complete with `own` – GDI cuts off their bottom row (`docs/re/text-rendering.md`).
**Result A (2026-10-24, windowed 2560×1440, DE + EN):** passed, no warning. With `own` the four bundled families log "own rasterizer", Dialog "GDI rasterizer"; every glyph-renderer line (height, capital height, line height, accents lowered, 61 of 61 fonts, 7249 glyphs) equals the `gdi` run except `fontbig36*`: font 8 px lower instead of 7 – GDI drew Uncial Antiqua's D a pixel lower with its bottom row cut off, the own D is whole and its tip sets the amount (human's choice: 8 px). Banner 412×39 px in both. Menu text otherwise matches. Logs `C:\dev\screenshots\t125-<de|en>-<own|gdi>.log`; crops `C:\dev\screenshots\planb\10-menu-DE-…`, `11-laden-DE-x4-…`, `12-menu-EN-…` (GDI left, own right).
**Result B (2026-10-25, windowed 2560×1440, real input, DE + EN + FR):** passed, no warning in any of the 18 runs (DE/EN: own and gdi × in game, options, NPC dialog, book/diary; FR: own and gdi × in game, book/diary). Load dialog, status window, letter, book, diary, options and picture settings with the own arrows, NPC dialog: same size, line breaks, colors and positions with `own` and `gdi` (side-by-side crops `C:\dev\screenshots\t125b\cmp\`, GDI left). FR status window: accented small caps (É in "Expérience", "Vitalité", "Réputation") complete in both – the GDI bottom-row cut does not occur on these screens. Sequence notes: in `T-Brief-Buch-Inventar` the EN (and FR) letter opens top left, so the DE close click (`0.3602 0.5979`) walks the hero out – book and diary there come from a run without the letter (inventory → chest tab → book 1 → Q).

## T1.24-D – Phase 1 on the Steam Deck (Proton, no dgVoodoo; Claude over SSH, automated in Game Mode)

Goal: the mod loads under Proton through `winmm.dll`, all hooks resolve, the overlay draws through WineD3D, and the glyph renderer's text looks like on Windows (`PLAN.md`, Decisions: plan B if Wine's rasterizing or emboldening differs clearly).

**Preparation (Claude, from Git Bash on Windows; the Deck on, SSH enabled):**
1. Build (`cmake --build build --config Release`) and `ctest`.
2. `tools/testrun/deck.sh deck@<deck-ip> check` – read only. Expected: no `div.exe` running, no `winmm.dll`/`dgVoodoo.conf`/`DDraw.dll` in the game folder (dgVoodoo is parked in `_ddmod_dgvoodoo_disabled`), renderer `slashed.cfg` = 0 (Direct3D 6 – the overlay draws only through `slash1.dll`), resolution in `config.div` and `slashed-d3d6.cfg`. Anything else: stop and tell the tester first.
3. `tools/testrun/deck.sh deck@<deck-ip> install` – backs up the savegames and configs to `divine_divinity/_backup_rr_<time>` (rule 7), installs with `LogLevel=debug`, writes `~/Desktop/rivellon-renewed-launch-options.txt`.
4. `tools/testrun/deck.sh deck@<deck-ip> testsaves T-Brief-Buch-Inventar T-Dialog-links T-Nebel T-draussen T-Trank-F2` – only saves whose name is not on the Deck yet.

**Preparation (tester, once):** Steam → Divine Divinity → ⚙ → Properties → Launch options: the line from the text file (`WINEDLLOVERRIDES="winmm=n,b" PROTON_LOG=1 %command%`; it replaces T0.6-D's `ddraw,d3dimm` override); then back to **Game Mode** – switching modes ends a running game (a leftover `init.cfg` that the backup does not have is removed before the next start). Nobody touches the Deck's mouse and keyboard during the run.

**Driving the Deck (Game Mode only):** `deck.sh … start` starts the game through Steam and waits for the overlay's first frame; `deck.sh … steps [--out <dir>] <steps>` sends input with `xdotool` on the game's Xwayland display and takes screenshots with `gamescopectl screenshot` (the game's own 1280×720 surface, no Steam UI) – only while gamescope reports app 214170 as focused, otherwise the shot is skipped (rule 11; it skipped once while Steam's start screen was still in front). Steps in `tools/testrun/deck-steps.sh`. A click must hold the button for a moment (an instant down/up is ignored by the game).

Verified sequences at 1280×720 (picture area 800×600 in the middle for the menus; fractions of the window):

| Goal | `steps` |
|---|---|
| Skip the intro | `key Escape wait 3 key Escape wait 3 key Escape wait 6 mouse 0.5 0.95` |
| Main menu entries (from the top: `key Up*9`; disabled entries are skipped) | Neu / Laden / Optionen at `click 0.5 0.504` / `0.5 0.56` / `0.5 0.671`; in game (all entries active) `key Up*9 key Down*2 key Return` = Laden, `Down*3` = Sichern, `Down*7` = Beenden |
| Options page row 1 / 2 / 3; Abbrechen; Zurück | `click 0.5 0.449` / `0.5 0.504` / `0.5 0.56`; `click 0.344 0.8375`; `click 0.5 0.8375` |
| Load/save list row n (n = 1 at the top) | `click 0.345 <(199 + 17·(n−1)) / 720>`, then ✓ `click 0.609 0.7125`; "really load?" ✓ `click 0.476 0.529`; save name field `click 0.445 0.7125` |
| Quit confirmation ✓ | `click 0.476 0.521` |
| Inventory chest tab / letter / book 1 / close reading window | `key i`, `click 0.305 0.181` / `click 0.125 0.256` (twice if the first click only ends the pause) / `click 0.170 0.253` / `click 0.498 0.651` |
| Diary / close it | `key q` / `click 0.770 0.851` |
| NPC dialog (`T-Dialog-links`) / close it | `click 0.02 0.5 wait 6` / `click 0.309 0.951` |

**Afterwards (Claude):** `deck.sh … fetch` (logs, Proton log); then the same screens from `dd-test` on Windows at 1280×720 (windowed, `config.div` + `slashed-d3d6.cfg` set and restored as in T1.6-W C; the verified `-Steps` sequences – only when the PC is free), cropped side by side for the tester. When the round ends, `LogLevel=info` on the Deck (`install --loglevel info` keeps the backup step).

**Expected result (log):** "Platform: Wine/Proton"; "EXE hash: … (verified)"; no "Signature … not found/unresolved", no hook error; "dgVoodoo settings: no dgVoodoo.conf in the game folder - nothing to do"; "Overlay: slash1.dll … (verified)", "Overlay: back buffer …", "Overlay: first frame"; "Glyph renderer: N of N fonts replaced"; "Fonts: family … - \"<face>\"" with the four OFL faces; "Font chain: using …" (which Tahoma Proton gives); "Input hook installed …", "Key repeat …"; "Fog hooks installed: fog on"; "Walk radius: 225 px"; "Double click run hook installed". In game: the screens look like the Windows ones; no crash, no "not correctly terminated" dialog at the next start.

**Undo:** `tools/testrun/deck.sh deck@<deck-ip> uninstall`, clear the launch options; savegames and configs from `_backup_rr_<time>` if needed.

**Steps (Claude, `steps` with a shot at each screen):** start; banner; main menu; options → picture settings (arrows), performance page (fog), sound page (short label); load dialog; `T-Brief-Buch-Inventar`: inventory, letter, book, diary; `T-Dialog-links`: NPC dialog, Ctrl+Shift+F2 twice, F10, lone Alt; save dialog: type, `hold BackSpace 1000`, `hold Left 1000`, Right, Delete, save as `T-Deck-Test` and load it again; `T-Nebel`; `T-draussen`: `press 0.5 0.3 1500` (144 px from the center), `press 0.5 0.1 1500` (288 px), click, double click; quit via the menu; start again.

**Result (2026-10-24, SteamOS Game Mode, Deck display 1280×800, game 1280×720, D3D6 through WineD3D):** passed so far – the side-by-side look against Windows is still open.
- Log: "Platform: Wine/Proton", EXE hash verified (same `div.exe` as on Windows), every signature and hook resolved, **0 warnings/errors** in all three starts; "dgVoodoo settings: no dgVoodoo.conf in the game folder - nothing to do"; overlay `slash1.dll` verified, back buffer 1280×720 RGB565, banner in the game's look. The four OFL faces load from `RivellonRenewed\fonts` (`AddFontResourceEx` under Wine works); the dialog face is Proton's "Tahoma Bold" (`C:\windows\Fonts\tahomabd.ttf`). Proton log: only exceptions of a 64-bit Proton helper process, none in the game.
- Screens (`C:\dev\screenshots\deck\t1.24\`): banner "RIVELLON RENEWED v0.01 ACTIVE" over the intro; main menu, options, sound page ("HINTERGRUND" clear of the arrow), own arrows, performance page "NEBEL EIN"; load list (Marcellus SC, "T-Händler-links" with its ä); letter and book in Fondamento with the wider line pitch; diary title "AUFTRÄGE" in Almendra SC; NPC dialog; mist in `T-Nebel`.
- Keys: FPS on/off ("90 FPS" top left), F10 and Alt "passed on as a normal key"; key repeat: Backspace held 1 s deletes 10 characters, Left held 1 s 11 steps (Windows: 10 – timing), Right and Delete act at once.
- Walk/run: 144 px held = walk, 288 px held = run, click = walk, double click = run (debug log).
- Save `T-Deck-Test` written and loaded again; quit via the menu ends the game in 2 s; the next start shows no "not correctly terminated" dialog.
- **Look against Windows (`dd-test` at 1280×720, same screens; crops `C:\dev\screenshots\deck\t1.24\compare\`, Windows top, Deck bottom):** the faces at weight 600 come out **clearly narrower and thinner under Proton** – menu (Uncial Antiqua: "INTRO ABSPIELEN" much shorter), interface (Marcellus SC: load list, picture settings), banner 378×35 px against 412×39 px on Windows. Capital heights are the same (the renderer measures them per platform; Wine picks other GDI heights, e.g. `fontbig36` 43 against 46), so the difference is GDI's synthetic bold – the plan-B case of the Decisions. Letter, diary and NPC dialog: the Windows run missed these screens (inventory/diary did not open), not compared yet.
- **The Deck's game folder has a third-party font tweak** (since 2026-08-24, not by the mod): 36 `.fnt` files in `fonts\` replaced by larger ones (e.g. `fontb3.fnt` = `fontb1.fnt`, the originals kept as `fontb3-.fnt`; `fontblack1/2`; the four `dialog_*.fnt` replaced by Verdana fonts `verdana_*_1.fnt`), the untouched originals in `fonts.bak\` (identical to the Steam files). Effects on the test: the small interface fonts are measured at the larger size; the four replaced dialog fonts are skipped by the glyph renderer without a log line (57 of 61 fonts, no summary line), so the NPC dialog shows the tweak's Verdana bitmap glyphs, not our Tahoma.

## T1.22-W – Ctrl+Shift+F2 in game and held Backspace in the save name (automated, real input)

**Steps:** windowed 2560×1440, `-NoIntro -Steps`: load the newest save (`T-Brief-Buch-Inventar`), `'chord Ctrl+Shift+F2'` twice, `'key F2'`, then the save dialog with the name field focused, `'type abcdefghijkl'`, `'hold Back 2500'`, a screenshot after each step. Comparison run (DE, with the mod at `LogLevel=debug` and without the mod): `'hold X 1500'`, then `'hold Back 2500'` in the name field.

**Result (2026-10-24, DE + EN):**
- Ctrl+Shift+F2 in game: passed – FPS on and off again (log "Overlay: FPS display on/off", "51 FPS" in the screenshot).
- With a potion on quick slot F2 (the tester's save `T-Trank-F2`, hero wounded; second run of T1.23-W, DE + EN): passed – after two Ctrl+Shift+F2 the health bar is unchanged (43 of 102 red pixels in DE, measured on the bar), F2 alone drinks the potion (97 of 102). In EN the bottom bar row lies below the screen edge in the windowed run and is captured black; the log shows both chords there. **Accepted by the tester.**
- Held Backspace deletes **one** character, with and without the mod (DE, EN with the mod) – the original game's behavior, not a mod bug. A held letter repeats normally (the field fills with x), and the debug log shows the game receiving every repeated `Char 08` – its text field ignores repeated Backspace only. Fixed as a comfort feature (T1.23-W).

## T1.23-W – Own key repeat for Backspace, Delete, Left, Right (`[Features] KeyRepeat`, `[Input]`; automated, real input)

**Which event acts:** `'down Back'`, screenshot, `'up Back'`, screenshot – the text is unchanged while the key is down, the character goes on release: the game's text fields act on key-up (`docs/re/input.md`).

**Steps:** windowed 2560×1440, `-NoIntro -Steps`, `LogLevel=debug`, save `T-Trank-F2` (`C:\dev\testsaves\de`, also copied to `dd-test-en`): load it, `'chord Ctrl+Shift+F2'` twice, `'key F2'`, `'hold Left 1500'` in the game world, then the save dialog with the name field focused: `'type abcdefghijkl'`, `'hold Back 1000'`, `'hold Left 1000'`, `'key Right'`, `'key Del'`, a screenshot after each step (name field crops at full resolution). DE + EN, defaults `RepeatDelay=300`, `RepeatRate=15`.

**Result (2026-10-24, DE + EN alike):** passed – Backspace held 1 s deletes 10 characters ("T-Trank-F2abcdefghijkl" → "T-Trank-F2ab"), Left held 1 s moves the cursor 10 places, one Right moves it one place at once, one Delete removes the character at once ("T-Tank-F2ab"); held Left in the game world changes nothing; log "Key repeat for Backspace, Delete, Left, Right: first repeat after 300 ms, then every 67 ms". An earlier version (Windows' repeats passed on as key-ups, Windows timing 500 ms / 30 per second) cleared the whole name in 2.5 s and was too slow to start and then too fast for the tester; `KeyRepeat=0` keeps the original (one character per press, on release). **Accepted by the tester** in the save dialog in full screen (borderless), with the default timing.

## T1.21-W – Black screen at the start of a new game (automated, real input)

**Steps:** windowed 2560×1440, `-NoIntro -Steps` with the "New game" sequence above, screenshots 3, 10 and 25 s after the ✓ click with no further input; the share of lit pixels (mean of R, G, B above 20) in the scene around the hero is measured (visible first room ≈ 37 %, black ≈ 0 %). Three starts each with the mod, with `[Features] Fog=0` and without the mod (`RivellonRenewed.asi` renamed away), DE and EN. **Before every release:** one start with the release build (`PLAN.md`, Decisions).

**Result (2026-10-24, DE + EN, both monitors 2560×1440):** 18 of 18 starts show the first room at 3, 10 and 25 s (37–38 % lit in every shot) – no difference between mod, `Fog=0` and no mod; the black screen of T2.0-W was not reproduced (`docs/re/open-questions.md`). Not covered: a start by Enter, other mouse positions.

**Full-screen probe (same day, DE, with the mod, `-FullScreen`):** in dgVoodoo's borderless full screen (`FullscreenAttributes = fake`) `PrintWindow` delivers the game window at 2560×1440 (no title bar, no desktop); the "New game" clicks hit unchanged (the script works in fractions of the client area, 2560×1440 in both modes); the first room visible after 3 s. The other sequences are not yet run in full screen (planned with the final release test, `PLAN.md` item 7).

## TRE.1-W – Play session with function coverage (by the tester)

**Preparation (done by Claude):** the play copy `C:\dev\dd-play` (copy of `dd-test` without saves and logs) has the current mod plus `ddmod_coverage.asi` and `ddmod_coverage.in` (mode `defer-window`: measuring starts when the game window appears, the mod runs normally). Each start writes its own `ddmod_coverage-<start time>.out` into the game folder. `LogLevel=info`. The saves made there are the tester's own and stay.

**Steps:** start `C:\dev\dd-play\div.exe` as usual (full screen is fine) and play about 30 minutes, as varied as possible; tick off what you did and note rough times:
1. load a save or start a new game (both if you like)
2. fight several opponents in melee, with a ranged weapon and with spells/skills
3. learn a skill or raise an attribute (level-up if possible)
4. trade with a trader (buy and sell), talk to a few NPCs
5. pick up, equip and drop items, use a potion, open a chest
6. diary pages, map with notes, options during play
7. save and load during play, quit the game via the menu

**Expected result:** the game behaves as usual (the tool only adds a few milliseconds the first time each function runs). If anything stutters, crashes or looks wrong, note when.

**Result, short check (2026-10-23, DE, full screen 2560×1440, ~11 min):** recording works – `ddmod_coverage-20261023-170915.out` written until the exit via the menu (2 828 of 8 152 functions hit), mod log without warnings, new game started and saved. Played: new game, walking, NPCs, trader, chest, letters/books, a cut-scene; no fight yet. Together with the five earlier runs: game code 2 703 of 5 865 functions (46 %), up from 39 %; `plate` 34 → 58 %, `itemstat` 67 → 92 %, `world` 42 → 52 %; `magic`/`weapon`/`script` still 3–4 % (no fight). Feedback in `docs/playtest-log.md`. Afterwards the play copy was set to 1920×1080 (`config.div` + `slashed-d3d6.cfg`) and `no fog` removed (human's wish; backups `*.pre-fullhd`).
**Result, second session (2026-10-23, DE, full screen 1920×1080, fog on, ~15 min):** recording works (`ddmod_coverage-20261023-173122.out`, 3 143 functions hit, exit via the menu), mod log without warnings. Played: fights with a skeleton and rats, cellar through the well, saves. All seven runs together: game code 3 079 of 5 865 functions (**52 %**, 55 % of the bytes); `script` 4 → 77 %, `magic` 3 → 21 %, `stats` 12 → 53 %, `effect` 18 → 76 %, `agents` 54 %; `weapon` still 3 % (no ranged weapon or projectile). Feedback in `docs/playtest-log.md`. Still open for a later session: ranged weapon, spells/skills, level-up.
**Result, third session (2026-10-23, ~17 min):** trading, intro from the menu (full screen, fine), credits, picture settings. Recording works (`ddmod_coverage-20261023-181853.out`, 3 037 functions). All eight runs: game code 3 220 of 5 865 functions (**55 %**, 58 % of the bytes); `diary` 86 %, `weapon` 15 %, `agents`/`world` 58 %.

## T1.20-W – Borderless full screen and the F10/Alt menu key (`[DgVoodoo] FullScreen`, `[Features] BlockMenuKey`; real input)

**Preparation:** build, `ctest` (668 checks: `SetConfValue` on `FullscreenAttributes` incl. the empty value with dgVoodoo's trailing space, `PlainMenuKeyMessage`); `Install-DdMod` in `dd-test` and `dd-test-en` (`LogLevel=debug` logs the sent window messages `WM_SYSCOMMAND`, `WM_ENTERMENULOOP`, `WM_ACTIVATEAPP`, `WM_STYLECHANGED` and "Menu key … passed on as a normal key").
**Steps:** full screen (`-FullScreen -NoIntro`; EN `-Intro -PressEscapeAt 4,8,12`): `'wait 16','state menu','mouse','key F10','wait 2','key 0x12','wait 2','alttab','wait 3','state away','refocus','wait 3','state back','key F10','wait 2','state end'` with `FullScreen=borderless` and (DE) `exclusive`; windowed (`-NoIntro`): `'wait 16','mouse','key F10','wait 2','shot after-f10','key 0x12','wait 2','state end'` with `BlockMenuKey=1` and (DE) `0`.
**Expected result:** log "dgVoodoo full screen: set to borderless" (`FullscreenAttributes = fake` in `dgVoodoo.conf`); the window stays a frameless popup of screen size after Alt+Tab; F10 and Alt give no `WM_SYSCOMMAND`/`WM_ENTERMENULOOP` with `BlockMenuKey=1`, but do with `0`.
**Result (2026-10-24, DE + EN, 2560×1440 on both monitors):** passed, no warning, no crash in any run. Baseline before the change (exclusive, mod without the fix): after Alt+Tab the window came back framed (`14CF0000`) and F10 then entered Windows' menu loop (`WM_ENTERMENULOOP`) – the softlock; in exclusive full screen itself F10 only sent `SC_KEYMENU` (no system menu). Borderless: style `94000000` before, during and after Alt+Tab in DE and EN; F10/Alt "passed on as a normal key", no `WM_SYSCOMMAND`. Exclusive with the fix: this time back in full screen after Alt+Tab (framed in the baseline run – not every time), F10/Alt without menu. Windowed (framed): `BlockMenuKey=1` no menu in DE + EN; `BlockMenuKey=0` `WM_SYSCOMMAND F100` + `WM_ENTERMENULOOP` on F10 (proof of the cause). Windows full-screen optimizations: no crash in nine runs (exclusive, borderless, windowed) – the player reports could not be reproduced here.

**Result of the feel test (2026-10-24, tester):** **accepted.** Borderless works throughout; switching back with Alt+Tab takes about a second – fine for the tester. F10 no longer stops the game.

**Feel test (by the tester, `dd-test`, full screen, any save):**
1. Play a bit, then Alt+Tab to another program and back (several times, also during a fight and a cut-scene); also click something on the second monitor and then back into the game (with `exclusive` this used to turn the game into a framed window). Does the game come back full screen and frameless at once? Does the switch feel fast? Any flicker, black screen or stutter?
2. Press F10, F11, F12 and Alt alone during play: the game must never stop.
3. Compare with `[DgVoodoo] FullScreen=exclusive` (restart after the change): which do you prefer? Does a cut-scene still get skipped by Alt+Tab (known exploit) in either mode?

## T1.19-W – Walk radius and double click run (`[Movement] WalkRadius`, `DoubleClickRun`; real input)

**Preparation:** build, `ctest` (655 checks: radius code run in a stand-in of the game's code – 1080 → 405 original, fixed values, back to original; double click detection incl. tick wrap; getter hook on a stand-in; circle, getter and held-run setter signatures unique at RVA `0xA65AE`/`0xA6FA0`/`0x41030`); `Install-DdMod` in `dd-test` and `dd-test-en` (`LogLevel=debug` logs "Held move: walk/run" and "Click move: walk/run").
**Steps (windowed 2560×1440, `-NoIntro -Steps`, `T-draussen` = 3rd in the load list):** load dialog, `'key Down 2','key Enter','wait 25'`, then `'press 0.5 0.4 1500'` (144 px from the center), `'press 0.5 0.3 1500'` (288 px), `'press 0.5 0.12 1500'` (547 px), `'click 0.62 0.5'`, `'dclick 0.38 0.5'`. Once more with `WalkRadius=original`.
**Expected result:** with 225: walk at 144 px, run at 288 and 547 px; with original (540 px at 1440): walk at 288, run at 547; single click walks, double click runs.
**Result (2026-10-24, DE + EN):** passed, no warning. Log as expected in both languages: walk / run / run with 225, walk / run with original; "Click move: walk" for the single click, "Double click at …" and "Click move: run (double click)" for the double click. Seen: **every release of a held button is also a click move** – it logs "Click move: walk" after each `press`, but in game the hero stops when the button is released (tester), so that click move does not carry him on.
**Result of the feel test (2026-10-24, tester):** double click run **accepted**. Stopping on release feels right – no change. Preferred radius: **175 px in DE at 2560×1440, 275 px in EN at 1920×1080** – neither the same pixels nor the same share of the height (12 % vs. 25 %); open whether window vs. full screen, the order of the tests or the larger view at 1440 explains it. Cross-check (DE started at 275, EN at 175, both full screen): the tester stepped through the values in both (with a test hotkey that was removed afterwards – the radius is set in the INI only) and settled on **225 px for every resolution** – the default.

**Feel test (by the tester, `dd-test`, any outdoor save, windowed or full screen):**
1. Hold the left button and move the cursor slowly away from the hero: where does walking switch to running? Try other values via `[Movement] WalkRadius` in `RivellonRenewed.ini` (restart the game after each change).
2. Single click far away: the hero walks. Double click at the same spot: he runs there.
3. Note which radius feels right – it becomes the default.

## T1.18-W – Own option arrows, "v0.01" in the banner, fog switch (`[Family.Menu] Arrows`, `[Features] Fog`; real input)

**Preparation:** build, `ctest` (604 checks: arrow masks, fog hooks on stand-ins, reader and writer signatures unique at RVA `0x8D6F0`/`0x8C6F0`); arrow variants as sheets first (`ddmod_fontpreview --arrows`, `C:\dev\screenshots\t1.18\01-pfeile-varianten.png`, human chose A); `Install-DdMod` in `dd-test` and `dd-test-en`; for the fog check `config.div` back to Steam's state (`no fog`, from `config.div.pre-fog`).
**Steps (windowed 2560×1440, `-NoIntro -Steps`):** banner at 12 s; Options → first entry → picture settings (arrows) and → "Leistung"/performance (Options: `'key Down 2','key Enter','wait 3','key Up 9','key Enter','wait 3','key Up 9','key Down','key Enter'`); there click the fog row's left arrow `0.5156 0.5855` (off) or right arrow `0.6897 0.5855` (on), then "Übernehmen"/"Apply" `0.6027 0.7234` – the keys move nothing on this page, and Esc does not leave it. Load `T-Nebel` (5th in the load list: load dialog, `'key Down 4','key Enter','wait 25','mouse 0.5 0.5','key Space'`) with `Fog=1` and `Fog=0`.
**Expected result:** banner "RIVELLON RENEWED v0.01 ACTIVE"; triangles beside the values in the original arrows' place and colors (gray at the end of a range); log `2 own arrows` per menu font; fog on with `Fog=1` despite `no fog`, off with `Fog=0` without it; switching it on the performance page and applying changes `[Features] Fog`.
**Result (2026-10-23, DE + EN):** passed, no warning. Arrows as on the sheet (variant A), gray/active states as in the original. Performance page shows "NEBEL EIN"/"FOG ON" with `no fog` in `config.div` and "AUS"/"OFF" with `Fog=0` and no line; `T-Nebel`: whitish mist beside the hero with `Fog=1`, none with `Fog=0` (both languages). Switching off and applying wrote `Fog=0` at once (the game writes `config.div` when a page is applied, not only on exit), switching on wrote `Fog=1` (DE). Screenshots `C:\dev\screenshots\dd-test*_20261023-19*`. Seen on the sound page: the wider Uncial Antiqua made "HINTERGRUNDGERÄUSCHE" run over the left arrow (the original fits, compared with `FontHook=0`) – all capitalized texts of DE/EN/FR measured (`ddmod_fontpreview --labels`) and the sound, performance and game pages checked in DE/EN/FR: this is the only overlap. **Part 2 – `[Fonts] ShortLabels`:** DE sound page shows "HINTERGRUND" clear of the arrow, log "2 text(s) shortened"; EN "0 text(s) shortened", page unchanged. **3D shadows** (performance page, switched on and applied, `T-Nebel` loaded): no visible difference at the hero or the trees in that scene (`open-questions.md`).

## T1.17-W – Capitals matched on all letters (menu, diary title) and the start banner in the game's look

**Preparation:** build, `ctest` (573 checks: median capitals ignore a taller H, `GdiFace::Capitals`, `LineMask`, `LowerAccent`/`ShrinkAccent`/`SqueezeTop`, `TopFit` defaults, menu and diary title on `CapReference::Median`, `[Overlay] BannerLook`, banner parts and the fallback without game fonts); `Install-DdMod` in `dd-test`/`-en` (`LogLevel=debug`), windowed 2560×1440; for screenshots of the banner `[Overlay] Status=always` (back to `banner` afterwards). Comparison sheets of the choice: `C:\dev\screenshots\t1.17\` (diary title B, menu B, banner 1 – human's choice).
**Part A (main menu, no input):** `Start-GameTest.ps1 -Intro -Seconds 30 -Count 1`, DE and EN.
**Part B (real input, only when the PC is free):** diary (Q) of the newest save and the picture settings page (arrows beside the values), each with the mod and with `[Features] FontHook=0`, DE + EN.
**Expected result:** log `Fonts: family Menu …`, `diary_head_1024 … (GDI height 50, capital height 29 …)`, `Overlay: banner in the game's look (… px)`; banner top left: "RIVELLON RENEWED" in the menu's face with gloss, "0.01" white and "ACTIVE" green in the load dialog's face; menu entries as large as the original's, lines apart; diary title at the original's height and position; options arrows beside their values.
**Result A (2026-10-23, DE + EN):** passed, no warning. Banner 405×39 px at the top left on the black border, as on the sheet (variant 1); the first frame is drawn during the intro, so the banner's 5 s start there. Menu (`fontbig36`, GDI height 46): entries at the original's size, neighboring lines apart, DE "INTRO ABSPIELEN" fits. EN "VIEW INTRO": the word gap looks narrow because Uncial Antiqua's W reaches far to the right (the face's own space). Screenshots `C:\dev\screenshots\dd-test_20261023-141601_01.png`, `dd-test-en_20261023-141633_01.png`.
**Result B (2026-10-23, real input, DE + EN, 2560×1440 windowed):** diary title at the original's height and place (capitals' dense rows 22–50 of the crop in original and mod, DE and EN; width within 6 px); options arrows beside their values as in the original. **Defect:** everything above the line top is cut off in menu entries and the diary title – "ZURUCK", "UBERNEHMEN" without dots, flat tops on Uncial's d/h/k/l, only the bottom row of the diary Ä's dots (`docs/re/text-rendering.md`). Screenshots `C:\dev\screenshots\t117-*`.
**Part C – `[Family.Menu]`/`[Family.DiaryTitle] TopFit` (DE, real input):** `none` / `lower` / `squeeze`, diary and picture settings each. Uncial Antiqua's left Ü dot touches the U's raised arm, so its dots cannot move closer; Almendra's Ä dots move down (13 accents in `diary_head_1024`). `lower`: everything complete, menu 7 px lower (`fontbig36`), diary title 6 px lower (Almendra's circumflexes); `squeeze`: position kept, Uncial's Ü dots almost vanish, ascenders and the Ä's dots flattened. A `shrink` variant (smaller face at the same place) needed GDI height 14 instead of 46 – dropped. Comparison sheets `C:\dev\screenshots\t1.17\04`–`07` (original, none, lower, squeeze). One run crashed on closing (`ntdll.dll` 0xC0000005, 14:41:58) and the next game hung on exit: `LowerAccent` wrote past a lone accent's mask (no letter under it) – fixed (such marks stay where they are), checked with a bounds-checked build over every Latin-1 glyph of both faces at heights 10–64. `Start-GameTest.ps1` failed on a game that ended at once (second instance) and skipped its restore – fixed.
**Result D (2026-10-23, real input, menu `lower`, diary title `lower` (Result B run) and `fit`):** passed, no crash, every game closed cleanly. DE + EN: main menu with banner, options page "ZURÜCK" and picture settings "ÜBERNEHMEN"/"DUNKELHEIT" complete (dots and ascenders), arrows beside their values. Diary title `fit` (DE + EN + FR, log: 12 accents moved close, 14 drawn smaller, 3 glyphs compressed, 0 px lower): at the original's place (ink from row 18 in original and mod, DE), Ä dots small and complete; FR shows "QUETES" without accent like the original. Sheet `C:\dev\screenshots\t1.17\08-tagebuch-fit.png` (original, lower, fit). **Human's verdict:** `lower` for both (diary title 6 px lower, letters at full size); `fit` stays an option.

## T1.16-W – Glyph renderer on the remaining screens (R4, automated)

**Preparation:** build, `ctest` (541 checks: menu family keeps `[`/`]`, no other family keeps a slot); `Install-DdMod` in `dd-test`/`-en` (`LogLevel=debug`); symbol slots of all 61 `.fnt` files checked with `ddmod_fontpreview --symbols` (only `fontbig24*`/`fontbig36*` have symbols: `[` = ▶, `]` = ◀).
**Steps:** windowed 1600×900, `-NoIntro -Steps` with the sequences in "Driving the game by keyboard": options menu and picture settings, key configuration, credits, `T-Händler-links` → trader dialog → trade window, skill panel, diary map with the "Quests" tooltip; each with the mod and with `[Features] FontHook=0` (original), DE + EN.
**Expected result:** every text in its family face at the original's size and color, nothing cut off; the options arrows as in the original.
**Result (2026-10-23, DE + EN):** passed after one fix. First run: the arrows beside the option values showed as plain `]` `[` in Uncial Antiqua – the menu fonts store the arrows in those slots; fixed by keeping the original glyphs there (`keepOriginal`). Afterwards all screens match the original in layout and color: menu capitals with gloss, key configuration and skill panel in Marcellus SC (very close to the original small caps), trade amounts ("51 x 1", `fontn5`), trader dialog in Tahoma Bold, credits, map tooltip. `fontbig24*` did not appear on any screen at 1600×900. **Comparison sheets for the human (rule 12):** `C:\dev\screenshots\t1.16\<DE|EN>-<01..08>-<screen>.png` (original left, mod right, 100 % crops). **Human's verdict:** accepted.

## T1.15-W – Language spot test FR / RU and the dialog face's file (R3, automated)

**Preparation:** build, `ctest` (539 checks: registry font list names incl. collections, Tahoma Bold found as `tahomabd.ttf` with its English name on a German Windows); `dd-test-fr`/`-ru` set to 1600×900 + HIGHDPIAWARE (backups `*.pre-1080`), `Install-DdMod -ResetIni`, `LogLevel=debug`, test save `T-Brief-Buch-Inventar` (German) copied into their empty `savegames\`; `dd-test`/`-en` `Install-DdMod`.
**Steps:** `-NoIntro -Steps`: main menu → load dialog → load the save → inventory, chest tab → letter → close it with its ✕ (`click 0.3252 0.5675`; Esc opens the game menu instead) → book 1 → diary (Q), screenshot at each stop. DE + EN: main menu only (`-Seconds 20 -Count 1`).
**Expected result:** log `Font chain: using "Tahoma" (installed font "Tahoma Bold", file C:\Windows\Fonts\tahomabd.ttf)`; FR: every text in its family face with correct accents; RU: "Glyph renderer: Russian edition - its Cyrillic bitmap fonts stay", texts in the original Cyrillic fonts.
**Result (2026-10-23, windowed 1600×900, all four copies):** passed, no warning. The log line as expected in DE, EN, FR, RU (glyph renderer and overlay). FR: menu in Uncial Antiqua, letter and book in Fondamento with é è ê à ç and apostrophes correct, diary title "QUETES" and entry "On ne vous a confié aucune quête …"; Œ/œ does not occur on these screens (coverage checked by `ctest`). RU: the German save loads in 1.0061; menu, letter, tooltip and diary in the original Cyrillic bitmap fonts. Screenshots `C:\dev\screenshots\fr-r3*`, `ru-r3*`, `dd-test*-r3*`.

## T1.14-W – Glyph renderer with the faces per family (`[Fonts] Renderer=glyphs`, automated)

**Preparation:** build, `ctest` (530 checks: family of every game font, line-height parser, defaults, bundled faces present and selected by GDI with every German/French character and no Cyrillic, game glyph decode round trip); `config.div` and `slashed-d3d6.cfg` of both copies backed up as `*.pre-1080` and set to 1920×1080 (laptop screen only); `Install-DdMod -ResetIni`, `LogLevel=debug`.
**Part A (main menu, no input):** `Start-GameTest.ps1 -NoIntro -Seconds 26 -Count 1`, DE and EN; once with `FontHook=0` for comparison.
**Part B (in game, real input, only when the PC is free):** load dialog → `T-Brief-Buch-Inventar` → inventory → letter (both pages) → book 1 → diary (Q) → NPC dialog (`T-Dialog-links`) → character sheet; screenshot at each stop, DE and EN; compare with `FontHook=0`.
**Expected result:** log "Fonts: family …" for the five families with the chosen faces, "61 of 61 fonts replaced"; every text in its family face, colors as the originals, menu with gloss; books with wider line pitch and re-paginated pages; nothing cut off or overlapping.
**Result A (2026-10-23, windowed 1920×1080, DE + EN):** passed – faces Marcellus SC / Fondamento / Uncial Antiqua / Tahoma / Almendra SC, 61 of 61 fonts, 7261 glyphs, 735–768 ms; menu capitals in Uncial Antiqua with gloss and texture, normal / highlight / gray as the originals. **Capture artefact, not a mod bug:** windowed at 1920×1080 on the 1920×1080 laptop screen, Windows scales the (not DPI-aware) game window by 125 % to about 2425×1404 – larger than the screen, so its right and bottom part lie outside it and some screenshots lose everything below y = 864 of the scaled window (last menu entry, version line). The human's own full-screen test showed the complete menu with the mod. Windowed screenshot runs need a window that fits on the screen (see PLAN.md, Current status).
**Result B (2026-10-23, windowed 1600×900 with HIGHDPIAWARE, real input, DE + EN):** passed, no warning in the log. Screenshots `C:\dev\screenshots\dd-test*_20261023-1106*` to `-1115*`. Load dialog: list, info panel and name field in Marcellus SC, colors as the originals, "71.64 GB" with its period. Status window (key T): every line fits the panel, red/blue/orange lines in the original colors. Diary (Q): title "AUFTRÄGE"/"QUESTS" complete in Almendra SC with the measured gradient and light rim; entry line in Fondamento dark blue. Letter and book: Fondamento at line height 18, lines no longer touch; EN letter breaks onto a second page (">>"), the DE letter window reaches below the icon bar exactly like the original (compared with `FontHook=0` in the same run – the original's text "500Goldstücke" has no space either). NPC dialog (`T-Dialog-links`): Tahoma Bold, yellow NPC line, gray answers with visible periods ("1.", "2."). Click points for 1600×900 (the inventory sits at the same absolute pixels as at 2560×1440): chest tab `0.5444 0.4778`, letter `0.4013 0.5367`, book 1 `0.4363 0.5344`; a click on another item while a reading window is open does nothing.
**Comparison sheets for the human (rule 12):** `C:\dev\screenshots\t1.14\<DE|EN>-<01..07>-<screen>.png` – original (`FontHook=0`, same sequences, same run conditions) left, mod right, 100 % crops: main menu, load dialog, status window, diary, letter, book, NPC dialog. **Human's verdict:** accepted. Change request: the diary title ("AUFTRÄGE"/"QUESTS") sits a little lower than the original and so reaches further onto the parchment (probably the face's metrics) – fix later (R4).

## T1.13-W – Own glyph tables, proof of concept (`[Experimental] GlyphTable`, automated)

**Preparation:** build, `ctest` (408 checks: game glyph and sprite layout incl. absolute row pointers, edge shading never 0, font list parser, loader signature unique at RVA `0x94440`); `Install-DdMod` in `dd-test` and `dd-test-en`; `GlyphTable=fontn1, fontn2, fontn3, fontblack1, fontblack2` (`FontHook=1` stays – the glyph table skips the font mode).
**Steps:** run 1 `GlyphTableLinePitch=100`, run 2 `=150`; each `-NoIntro -Steps` main menu → load dialog → load `T-Brief-Buch-Inventar` → inventory, chest tab → letter (and its 2nd page) → book 1, screenshot at each stop.
**Expected result:** log "Glyph table: <font> - <n> glyphs replaced …" for the five fonts; those texts in Tahoma at the original's capital height and color, the rest in the original bitmap fonts; at 150 % a wider line pitch where the layout reads the font's line height.
**Result (2026-10-22, windowed 2560×1440, DE + EN):** passed. 130–131 glyphs per font replaced, no crash, no warning. Run 1: selected save name in the load dialog, item tooltips ("Manuskript", "Buch") and letter text drawn from the own glyphs, edges blended into the paper tone, spacing by the own advance; at the original 12 px pitch the letter's Tahoma lines overlap (expected). Run 2 (line height 12 → 18, 13 → 19): load list lines further apart, letter and book text with the wider pitch, and **the game's page break follows** – the letter gets a second page (">>"), the book spreads over more pages; EN the same (reading window top left as known). Copies reset to `GlyphTable=` (off), `GlyphTableLinePitch=100`.

## T1.12-W – Overlay start banner and FPS toggle (`[Overlay] Status`, `[Keys] ToggleFps`)

**Preparation:** build, `ctest` (386 checks: `Status` parsing, banner timing); `Install-DdMod` in `dd-test` and `dd-test-en`, then `Set-ModIniValue` `[Overlay] ShowFps=0` (the install keeps the old value 1) and `LogLevel=debug`.
**Part A (automated, posted keys – safe while the PC is in use):** `[Keys] ToggleFps=F2` (posted keys carry no modifiers), then
```powershell
.\tools\testrun\Start-GameTest.ps1 -GameDir <copy> -NoIntro -PostKeyAt 20,30 -PostKeyVk 0x71 -Seconds 8 -Count 4 -Interval 7
```
(EN copy: same with `-Intro` left out, it has no intro). Compare screenshot times with the log's "Overlay: first frame".
**Part B (automated):** `[Overlay] Status=always`, `ShowFps=1`, one screenshot – the old permanent display; `Status=off` – nothing at all.
**Part C (real input, only when the PC is free):** default `ToggleFps=Ctrl+Shift+F2`, `-PressHotkeyAt 20 -HotkeyVk 0x11,0x10,0x71`.
**Expected result:** log "Overlay: status banner (5 s), FPS off", "Hotkey ToggleFps: …", "Overlay: first frame", one "Overlay: FPS display on/off" per key. Screenshots: within 5 s of the first frame only "Rivellon Renewed 0.01 active"; later nothing; after the first toggle only "<n> FPS" in the corner; after the second nothing again. Part B: status + FPS permanently / no overlay.
**Result A (2026-10-21, windowed 2560×1440, DE + EN):** passed – log lines as expected, both toggles on the main thread; screenshots (full-resolution crops) ~2 s after the first frame: banner; ~8 s: nothing; after the first F2: only "44 FPS" (DE) / "40 FPS" (EN); after the second: nothing. Note: the first frame comes ~8.5 s after the start (game window), so screenshot times must start at ~11 s.
**Result B (2026-10-21, DE):** passed – `always` + `ShowFps=1`: status line and FPS as before; `off` + `ShowFps=0`: no overlay, log "status off, FPS off". Copies reset to `banner`, `ShowFps=0`, `ToggleFps=Ctrl+Shift+F2`.
**Result C (2026-10-21, DE + EN, real input):** passed – two Ctrl+Shift+F2 chords each, logged as "Hotkey ToggleFps (Ctrl+Shift+F2)" on the main thread, FPS on ("40 FPS" / "41 FPS" in the screenshot) and off again; no stray character or menu.

## T1.11-W – dgVoodoo watermark switch (`[DgVoodoo] Watermark`, automated)

**Preparation:** build, `ctest` (376 checks: value replaced in `[DirectX]` only, alignment and CRLF kept, `[Glide] 3DfxWatermark` and comments untouched, missing value, no line end, unknown key); `dgVoodoo.conf` of `dd-test` and `dd-test-en` backed up to the scratchpad; `Install-DdMod` in all four copies (INI rebuilt with `[DgVoodoo]`, old values kept). `Start-GameTest.ps1` now restores only `FullScreenMode`/`AppControlledScreenMode`, so the mod's change survives the run.
**Steps:** A – DE, conf `false`, `Watermark=1`; B – DE, `Watermark=0`; C – EN, conf set to `true` by hand, `Watermark=0`; D – DE, conf `true`, `Watermark=keep`. Each `-NoIntro -Seconds 15 -Count 1`; bottom corners cropped at full resolution.
**Expected result:** log "set to on/off in dgVoodoo.conf" (no "already loaded" warning) or "left as in dgVoodoo.conf"; the conf keeps the new value after the run; watermark visible exactly when the value is `true`.
**Result (2026-10-20, windowed 2560×1440, DE + EN):** passed – A: "set to on", conf `true`, "dgVoodoo / Divine Divinity-V1.0062a" bottom right; B and C: "set to off", conf `false`, no watermark; D: "left as in dgVoodoo.conf", file hash unchanged, watermark visible. Afterwards both confs identical to the backups, all copies on `Watermark=0`.

## T1.10-W – Rename to Rivellon Renewed and migration of the working copies (automated)

**Preparation:** build (`build/Release/RivellonRenewed.asi`), `ctest` (365 checks: log rotation and archive in `RivellonRenewed\logs`, default font chain points at `RivellonRenewed\fonts\`); old logs of all four copies copied to `C:\dev\screenshots\ddmod-logs-<copy>`.
**Steps:** in `dd-test`, `dd-test-en`, `dd-test-fr`, `dd-test-ru`: `Uninstall-DdMod`, `Install-DdMod` (migrates `ddmod.ini`), `Set-ModIniValue … LogLevel debug`; then `Start-GameTest.ps1 -GameDir C:\dev\dd-test -NoIntro -Seconds 15 -Count 1` and a full-resolution crop of the overlay.
**Expected result:** no `ddmod*` file left except `ddmod.ini.migrated` (deleted after the check); the new INI keeps the old values and has the keys older INIs lacked (`LogHistory`, `Spacing`, `Colors`); log "Rivellon Renewed 0.01 loaded", "Config: …\RivellonRenewed.ini"; overlay "Rivellon Renewed 0.01" + FPS.
**Result (2026-10-20, DE windowed 2560×1440):** passed – all four copies migrated (FR/RU INIs gained the three keys, chain moved to `RivellonRenewed\fonts\`); log and overlay as expected (48 FPS, Tahoma 24 px). EN/FR/RU not started (same build, only file names changed).

## T1.9-W – Original text colors in the font mode (`[Fonts] Colors`, automated)

**Preparation:** build, `ctest` (364 checks: color table against the `.fnt` files of `dd-test`, 565 conversion, descriptor signature against `div.exe`); `Install-DdMod` in both copies; `RivellonRenewed.ini` changed only for the run and restored (script in the session scratchpad).
**Steps:** three variants per copy – `FontHook=0` (bitmap fonts, reference), `Colors=game`, `Colors=original` – each with two `-Steps` walks from "Driving the game by keyboard": main menu, load dialog, load `T-Brief-Buch-Inventar`, inventory, letter, book 1; then `T-Dialog-links` NPC dialog. Comparison sheets (crops of the same window region, 2×, variants stacked) and the most frequent text color per probe line measured in the screenshots.
**Expected result:** log "Font colors: original colors set for all 61 fonts"; with `original` every measured text color equals the bitmap font's color; `game` shows the old colors.
**Result (2026-10-20, windowed 2560×1440, DE + EN):** passed and accepted by the human from the sheets; no save written. Log in every `original` start: "Hook 'AddFontDescriptor': enabled", "Font colors: original colors set for all 61 fonts". Measured most frequent text color (bitmap / game / original, identical in DE and EN):

| Probe | Bitmap | Game | Original |
|---|---|---|---|
| NPC line (`dialog_yellow`) | `#DEBA18` | `#FFFF00` | `#DEBA18` |
| Answers (`dialog_grey`) | `#ADAAAD` | `#7B7D7B` | `#ADAAAD` |
| Save list (`fontgo`) | `#E7B231` | `#C6C739` | `#E7B231` |
| "Spiel laden" (`fontgr`) | `#7BBA42` | `#00FF00` | `#7BBA42` |
| Main menu (`fontbig36`, textured) | `#C6BA8C` | `#FFFF7B` | `#C6BA94` (mean of the texture) |

- A first build passed the measured 8-bit colors straight on and came out one 565 step darker (`#D6B610`, `#E7AE29`): the game's conversion rounds down (`text-rendering.md`, Colors); fixed by `ToDescriptor`, all probes exact afterwards.
- Sheets `C:\dev\screenshots\colors\sheet-{de,en}-{menu,load,inv,letter,book1,npc}.png`: dialog, load dialog and menu now in the original hues (gold, warm gray, green, parchment instead of yellow, gray, neon green); books and letters (black ink) look as before – the edge tone (paper instead of gray) is hardly visible. The menu capitals keep only their mean color; the bitmap's texture and highlights are not reproduced by a two-color ramp.

## T1.8-W – Language check FR / RU (automated)

**Preparation:** Steam switched to French, then Russian, by the human; each time the installation copied to `C:\dev\dd-test-fr` / `dd-test-ru` and set up like `dd-test-en` (dgVoodoo2, 2560×1440, `startup logo 0`, `Install-DdMod -ResetIni`); file comparison against a hash list of the German installation (`docs/re/game-version.md`). `ctest`: 206 checks (including edition by hash and `[Fonts] Charset`).
**Steps:** `Start-GameTest.ps1 -GameDir <copy> -Seconds 15 -Count 1`, main menu screenshot; RU additionally with `use8bitfont 0` set by hand in `config.lcl` (restored afterwards) to check `Charset=auto`.
**Result (2026-10-19, windowed, 2560×1440):** passed.
- FR (same EXE as DE/EN): "verified", font mode forced `use8bitfont` 0, French main menu in Tahoma Bold.
- RU (EXE 1.0061): log "Russian edition 1.0061 (Steam) - best effort …" and "Russian edition 1.0061 detected - font mode not supported, using original fonts"; main menu in the original Cyrillic bitmap fonts; input hook and overlay normal.
- RU with `use8bitfont 0` by hand: without a charset change Latin-1 garbage; with `Charset=auto` the log says "Game fonts: charset 204 ([Fonts] Charset=auto)" and the menu shows correct Cyrillic in Tahoma Bold.
- DE regression after the change: font mode and Tahoma as before.

## T1.7-W – Game texts through GDI and the font hook (`FontHook=1`)

**Preparation:** build, `ctest` (206 checks, including the font chain and the font mode hook with its signature against the working copy's `div.exe`), `Install-DdMod -GameDir <copy> -ResetIni` (also copies `RivellonRenewed\fonts\`). `config.lcl` stays as shipped (`use8bitfont 1`): with `FontHook=1` the mod switches the game to its GDI text path in memory (`docs/re/text-rendering.md`, "Font mode").

**Part A (automated, windowed, posted Enter – no real input):** `use8bitfont 0` and `1` in `config.lcl`, `LogLevel=debug`, main menu at 14 s, posted Enter at 16 s (opens character creation), second screenshot at 24 s; script in the session scratchpad, restores `config.lcl` and `RivellonRenewed.ini`.
**Result A (2026-10-18, DE + EN, dgVoodoo2, 2560×1440):** passed.
- `use8bitfont 1`: all 61 `.fnt` files opened at startup, no GDI font.
- `use8bitfont 0`, `FontHook=0` (log only): no `.fnt` opened; 61 `CreateFontIndirectA` calls, all "SimSun" weight 600, one per bitmap font in the same order; main menu, hint line, version line and character creation drawn in SimSun (table in `docs/re/text-rendering.md`).
- `use8bitfont 0`, `FontHook=1`: every call becomes "Tahoma" (log "Font chain: using \"Tahoma\" (installed font)"); main menu and character creation in Tahoma Bold, colors and layout unchanged, overlay unaffected.
- Fallback: `Chain=No Such Face 4711, DejaVu Sans Condensed @ …` → "not available (GDI substitutes \"Arial\")", then "using \"DejaVu Sans Condensed\" (file …\RivellonRenewed\fonts\DejaVuSansCondensed-Bold.ttf)"; menu in DejaVu, overlay too.

**Part A2 (automated, windowed, font mode in memory):** `config.lcl` unchanged (`use8bitfont 1`), default `RivellonRenewed.ini`; `Start-GameTest.ps1` (DE with three posted Esc for the intro), screenshot of the main menu; hashes and timestamps of `config.div`/`config.lcl` before and after.
**Result A2 (2026-10-19, DE + EN, dgVoodoo2, 2560×1440):** passed.
- `FontHook=1`: log "Hook 'FontInit': enabled", "Font mode hook installed … (settings pointer at 0x0065B3D0)", "Font mode: use8bitfont forced to 0 (was 1) while the game creates its fonts", "Font chain: using \"Tahoma\""; main menu in Tahoma Bold (same look as part A with `use8bitfont 0` in `config.lcl`).
- `FontHook=0` (counter-check): no font hooks, main menu in the original bitmap capitals.
- `config.div` and `config.lcl`: same SHA-256 and timestamp before and after all four runs. The script closes the game without a clean exit, so the game did not rewrite `config.div` – the write-back on a clean exit is checked in part B step 4.

**Part B (by the tester, in game, DE then EN – needs real mouse input):**
1. Back up `savegames\` of the copy (rule 7). Leave `config.lcl` as it is (`use8bitfont 1`) – no manual `use8bitfont 0` any more. In `RivellonRenewed.ini` set `LogLevel=debug` (keep `FontHook=1`).
2. Start the game (full screen as usual), load a save. Visit these screens and take **one screenshot each** (Win+Alt+PrtSc or Game Bar – only the game window): a dialog with an NPC (with answer options), a book or letter, the diary, an item tooltip, the inventory, a trader, the character sheet/skills, the map, the options menu, a loading screen.
3. On each screen check: umlauts and ß (DE) / accents correct? Text fits its box (not cut off, no overlap)? Anything still in the old bitmap look?
4. Quit **via the menu** (so the game rewrites `config.div`). Start once more with `FontHook=0` (original bitmap fonts) and repeat the dialog and the book screenshot for comparison; quit via the menu again. I then check the write-back: **DE** (normal case, key only in `config.lcl`): `config.lcl` unchanged, `config.div` has no `use8bitfont` line. **EN** (worst case): `config.div` additionally holds `use8bitfont 1` (added by me; backup `config.div.pre-t17b` in the copy), so the game writes the key back on a clean exit – it must still read `use8bitfont 1` afterwards, not 0. Afterwards I restore the EN `config.div` from the backup.
5. Optional: `GameFontSize=115`, dialog and book again – does the text still fit?
6. Reset: `FontHook=1` (or 0 if you prefer the bitmap fonts), `LogLevel=info`. Send me `RivellonRenewed.log` and `RivellonRenewed.prev.log` of both copies and tell me the screenshot times.

**Result B (2026-10-20, tester, full screen, DE + EN):** partly failed; the three bugs are fixed or explained in part C. EN behaves like DE in every point below; all eight DE test saves load in EN (also `T-Händler-links`), and names typed with the German keyboard layout (y, ä) come out right.
- Screens with `FontHook=1` (screenshots 17:11–18:35, DE + EN): main menu, load/save dialog, NPC dialog, letter, book, diary, item tooltips, inventory, trader, character sheet, skills, loading screen, key configuration, minimap (world map not opened). All game text in Tahoma Bold, nothing left in the bitmap look, umlauts and ß correct, texts fit their boxes; `k`/`t` are fine at full resolution. Font per screen: table in `text-rendering.md`. The menu capitals lose their ornate look (Consequences there). The diary tab title ("QUESTS"/"AUFTRÄGE", `diary_head_1024`) was almost black on black near the top screen edge; automated comparison (DE, windowed, key Q): the game puts it there with the bitmap fonts too, and with `Colors=original` it is readable brown – only the ornate face is missing (per-family font choice). Spacing then was still `game` ("lightning", "lavendel" = the "Li" case, fixed by `Spacing=font`).
- **Backspace** in the save dialog inserts boxes with `FontHook=1` (held down: a row of boxes); deletes with `FontHook=0` and without the mod → caused by the font mode.
- **Periods after digits seem dropped** with `FontHook=1` ("7337 GB" instead of "73.37 GB", "1" instead of "1.").
- `GameFontSize=115`: fonts created larger (log); the tester saw no difference to 100.
- Step 4 (write-back): the game did **not** rewrite `config.div` on quitting via the menu (DE and EN, timestamps unchanged), so the check is inconclusive; EN `config.div` restored from `config.div.pre-t17b`.
- Fog: `no fog` is in Steam's own `config.div` – not a mod issue. Black screen at the first "new game" not reproducible without the mod.

**Expected result:** every text in the game is drawn in Tahoma Bold (FontHook=1) or in the original bitmap fonts (FontHook=0); the log shows "use8bitfont forced to 0 (was 1)" once per start with `FontHook=1` and lists "GDI glyphs of font …" lines per screen, which I map to the font families; boxes may clip – that decides whether `GameFontSize` stays at 100 for v0.01.

**Part C (automated, real input via `-Steps`, PC free; fixes for the part B bugs):** `ctest` 242 checks (control characters, spacing modes, pair spacing signature against `div.exe`); `savegames\` of both copies backed up to the scratchpad; sequences from "Driving the game by keyboard"; `RivellonRenewed.ini` changed only for the run and restored. Save name field: click, `type ab`, `key Back`, `hold Back 1500`, `type xy`, screenshot after each; load dialog screenshot with `[Fonts] Spacing` = `game`/`gap`/`font` and with `FontHook=0`.
**Result C (2026-10-20, windowed 2560×1440, DE + EN):** passed; no save written (no file in `savegames\` changed).
- Without the fix (DE): reproduced – one Backspace changes nothing (cursor stays), held for 1.5 s the field fills with boxes.
- With the fix: one Backspace deletes one character, held down it deletes one more (same as `FontHook=0`, which was checked in the same run), typing goes on normally; no boxes (DE + EN).
- Spacing, first round (load dialog): `game` draws "71.47 GB" so that the period merges with the foot of the "1"; `gap` shows the period; `font` (first version) spaced too wide. The human first chose `gap`, but the tester's letter showed "Lieber" as "lieber" (the "i" covers the foot of the "L" – also with `gap`). Second round with `font` corrected (advance only) in five scenes – load dialog, letter, two books, NPC dialog; sheets `C:\dev\screenshots\fonts\spacing-cmp-*.png` (DE: original / gap / font) and `spacing-en-*.png` (EN: font): `font` fixes every case ("Lieber", "71.47", "1.", "2."), width about as before, same line breaks or one word moved, all save names fit; `gap` still hides "2." in the dialog. **`font` is the default.**
- `GameFontSize=150` (DE): clearly larger text in menu and load dialog, but the line positions stay – names get cut ("T-Brief-Buch-Inven…") and lines overlap ("71.48 GB" over "Festplattenspeicher"). At 130 (tester, part D) letters still fit their line pitch, but the last line of a full book page is cut. The option works; the game's layout does not grow with it.

**Part D (by the tester, final acceptance of the part C fixes, full screen as normally played – DE in `C:\dev\dd-test`, then EN in `C:\dev\dd-test-en`):** both copies have the current build and the default `RivellonRenewed.ini` (`FontHook=1`, `Spacing=font`, `GameFontSize=100`, `LogLevel=info`). Nothing is saved in this test.
1. Start `div.exe`, load `T-Dialog-links` (no save backup needed – the `C:\dev` copies hold only test saves).
2. **Backspace:** Esc → Save (DE "Sichern"). Click into the name field at the end of the name, type `abc`. Press Backspace **once** → the `c` disappears. **Hold** Backspace for about 2 s → one more character disappears (same as the original game), no boxes anywhere. Type `xy` → appears normally. Leave the dialog with **Esc** (do not save).
3. **Periods:** in the save or load dialog look at the free disk space ("71.47 GB" – period visible between the digits) and the dates in the list ("20.10.2026"). Back in the game talk to the NPC on the left: numbered answers end with a visible period ("1.", "2."). Main menu bottom right: "V1.0062a" with its period. Letter spacing in general: does anything look too tight or too wide compared with before?
4. **GameFontSize:** quit via the menu, set `GameFontSize=130` in `RivellonRenewed.ini`, start again: main menu and load dialog text clearly larger (overlap and "…" are expected – the game's layout does not grow). Quit, set `GameFontSize=100` again.
5. Tell me per copy: passed / what looked wrong, with screenshot times if any (Win+Alt+PrtSc, only the game window).

**Expected result:** step 2 without boxes and with normal deleting; step 3 every period visible and separate from the digit before it; step 4 larger text at 130, the original size at 100.
**Result D (2026-10-20, tester, DE, with `Spacing=gap` at the time):** Backspace passed; periods passed; `GameFontSize=130` works (text larger, still fits letters and dialogs). The letter screenshot showed "Lieber" read as "lieber" – fixed by `Spacing=font` (part C, second round, automated in DE + EN); the tester's look at `font` in game is still open.

## T1.6-W – Minimal overlay on Windows

**Preparation:** build, `ctest -C Release --test-dir build --output-on-failure` (168 checks, including pixel formats, blending, clipping, glyph cache, GDI glyphs and the `.fnt` reader), `Install-DdMod -GameDir <copy> -ResetIni` (adds `[Features] Overlay=1` and `[Overlay]`).

**Part A (automated, windowed, no input):**
```powershell
.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test -NoIntro -Seconds 15 -Count 2 -Interval 10
.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test-en -NoIntro -Seconds 15 -Count 2 -Interval 10
```
**Expected result:** `RivellonRenewed.log` shows "Overlay: slash1.dll … (verified)", "Hook 'DllSlashedEndFrame': enabled", "Overlay: back buffer <w>x<h> <format>" and "Overlay font: …"; the screenshots show "Rivellon Renewed 0.01" and "<n> FPS" in the top-left corner on a translucent dark panel.
**Result A (2026-10-18, dgVoodoo2, 2560×1440 windowed):** passed – DE and EN: back buffer 2560x1440 **RGB565** (dgVoodoo2 gives 16 bit also windowed), Tahoma 24 px, overlay visible in the main menu with 41–43 FPS, menu unchanged. A run without dgVoodoo2 (Windows ddraw, to reach the 32-bit path) is not possible on this PC: the game exits after 1 s before its window appears, before the mod does anything (`docs/re/open-questions.md`).

**Part B (by the tester, full screen as normally played – DE, then EN):**
1. Back up `savegames\` of the copy (rule 7). Start `C:\dev\dd-test\div.exe` normally (full screen, with intro).
2. Intro video: no overlay expected (videos are not drawn by the renderer). Skip it with Esc.
3. Main menu: overlay top-left – white "Rivellon Renewed 0.01", yellow "<n> FPS" below, on a dark see-through panel. Note the FPS value.
4. Load a save (or start a new game). Check: overlay readable over the game scene (the scene shows through the panel), no flicker, no colored garbage, FPS plausible; walk around for a minute, open inventory (**I**) and map (**M**), talk to someone – the overlay stays on top and the game behaves as without the mod.
5. Alt+Tab out and back: overlay still there.
6. Quit via the menu. Then set `Overlay=0` in `RivellonRenewed.ini`, start once more: no overlay, log shows "Overlay disabled". Reset to `Overlay=1`.
7. Repeat steps 1–4 in `C:\dev\dd-test-en`. Send me both `RivellonRenewed.log` files (and `RivellonRenewed.prev.log`) plus, if possible, a screenshot in game (Win+Alt+PrtSc or Xbox Game Bar – lands in `%USERPROFILE%\Videos\Captures`; only the game window).

**Expected result:** steps as described; `RivellonRenewed.log` has one "Overlay: back buffer 2560x1440 RGB565" line (full screen), no warnings or errors from the overlay, and the game runs as without the mod. Report anything odd (flicker, wrong colors, stutter, crash, text cut off).

**Result B (2026-10-20, tester, full screen, DE + EN):** passed with notes – overlay readable over menus and the game scene, stays after Alt+Tab (the game comes back as a framed window – also without the mod, T1.5-W); `Overlay=0` → no overlay, log "Overlay disabled". `[Overlay] FontSize=100` by mistake gave a 96 px overlay (works as configured). Finding: saves made with the overlay on show it on their loading screen (save screenshot, `open-questions.md`).

**Part C (other resolution – DE only):** the game cannot change its resolution while running; it is set before the start.
1. Preparation by me: back up `config.div` and `slashed-d3d6.cfg` of `C:\dev\dd-test` as `*.pre-t16c`.
2. Set 1920×1080: `width`/`height` in `config.div` and the two DWORDs at offsets 0x0C/0x10 of `slashed-d3d6.cfg` (same effect as `configtool.exe`); start the game (windowed by script, full screen by the tester).
3. Main menu and a loaded save: overlay in the top-left corner, text proportionally smaller than at 2560×1440 (font size follows the back buffer height), readable, no garbage. Quit via the menu.
4. Restore by me: both files from `*.pre-t16c` (the copy stays at 2560×1440).

**Expected result:** `RivellonRenewed.log` shows "Overlay: back buffer 1920x1080 RGB565" and "Overlay font: \"Tahoma\" 18 px" (back buffer height / 60, at 2560×1440 it is 24 px).

**Result C (2026-10-21, DE, 1920×1080):** passed. Resolution set directly in `config.div` and `slashed-d3d6.cfg` (offsets 0x0C/0x10) instead of `configtool.exe`, backed up as `*.pre-t16c` and restored afterwards. Automated: windowed and full screen, each main menu → load `T-Brief-Buch-Inventar` → scene, inventory (**I**), map (**M**) via `-Steps`; both logs show "back buffer 1920x1080 RGB565" and "Overlay font: \"Tahoma\" 18 px (line height 22)", all keys reached the game, no crash, no save written. Windowed screenshots: overlay top-left in menu, scene, inventory and map, smaller than at 2560×1440, readable, 60 FPS, no garbage (full screen gives no screenshots – `PrintWindow` black). The tester then played full screen at 1920×1080: everything fine. Observations: the game is much easier to read at 1920×1080 because all sprites are larger; the unexplored area (fog of war, not the ground fog) covers much of the screen – the reveal radius probably stays the same at every resolution (`open-questions.md`).

## T1.5-W – Input hook and test hotkey on Windows

**Preparation:** build, `ctest -C Release --test-dir build --output-on-failure` (103 checks, now including the `[Keys]` parser), `Install-DdMod -GameDir <copy> -ResetIni`.

**Part A1 (automated, posted keys – no real input, safe while the PC is in use):** set `TestHotkey=F3` in the copy's `RivellonRenewed.ini` (posted messages carry no modifier state), optionally `LogLevel=debug`; afterwards `Install-DdMod -GameDir <copy> -ResetIni`.
```powershell
.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test -PostKeyAt 12,13 -PressEscapeAt 14,18,22 -Seconds 28
.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test-en -Intro -PostKeyAt 12,13 -PressEscapeAt 14,18,22 -Seconds 28
.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test -FullScreen -PostKeyAt 10,16,22 -Seconds 28
```
**Part A2 (automated, real input via `SendInput` – only when nobody uses the PC):** default `RivellonRenewed.ini` (`Ctrl+Shift+F3`).
```powershell
.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test -NoIntro -PressHotkeyAt 12,14 -Seconds 16
```
**Expected result:** `RivellonRenewed.log` shows "Game window found … thread <main thread>", "Hook manager ready", "Hotkey TestHotkey: …", "Input hook installed on thread <main thread>" and one "Test hotkey pressed" per key/chord, logged by the main thread. Esc still skips the intro (keys we do not bind reach the game). With `InputHook=0`: "Input hook disabled", no "Test hotkey pressed".
**Result A1 (2026-10-18, dgVoodoo2, 2560×1440):** passed. DE windowed: 2/2 hotkeys on the main thread, Esc skipped the intro to the main menu. EN windowed: 1/1 (the English copy plays no intro – `startup logo 0` in its `config.lcl`, `docs/re/game-version.md` – so the Esc pass-through is verified in DE only). DE **full screen**: 3/3 hotkeys on the main thread (the debug trace shows the window procedure going from `DDRAW.dll` to generated code – the hook is not affected); the screenshot was skipped as intended (full screen).
**Result A2 (2026-10-18, real input, windowed, `-NoIntro`):** passed – DE 2/2 and EN 2/2 "Test hotkey pressed" on the main thread; DE with `InputHook=0`: "Input hook disabled", the chord had no effect.
**Result `-Intro` (2026-10-18, EN):** intro plays (Larian logo at 8 s), three posted Esc reach the game and skip it to the main menu – the pass-through of unbound keys now holds for EN too; `config.lcl` restored unchanged.

**Part B (by the tester, German copy, full screen as normally played):**
1. Back up `savegames\` (rule 7). Start `C:\dev\dd-test\div.exe` normally (full screen, with intro).
2. During the intro press **Ctrl+Shift+F3**, then **Esc** (skip the video).
3. In the main menu press **Ctrl+Shift+F3** twice.
4. Load a save. Press **Ctrl+Shift+F3**, then check that normal keys still work: **I** (inventory), **M** (map), **Space** (pause), **Ctrl+F11** (screenshot), Alt+Tab out and back, then **Ctrl+Shift+F3** again.
5. Optional: Options → change the resolution, then **Ctrl+Shift+F3** again.
6. During a loading screen (new game, loading a save): press **Ctrl+Shift+F3** once and note the time. Expected: one "Test hotkey pressed", possibly a few seconds late (the game takes queued keys off the queue after loading). Seen in T2.0-W B2 (EN): 2 presses during the first loading screen, both recognized.
6. Quit via the menu (not Alt+F4) and send me `C:\dev\dd-test\RivellonRenewed.log`.

**Expected result:** one "Test hotkey pressed" line per press (also after Alt+Tab and the resolution change); all game keys behave as without the mod; nothing happens in the game when the hotkey is pressed. (No "Rivellon Renewed unloading" on a normal exit: the process is terminating, so the mod only closes the log.)
**Result with the old window-procedure subclass (2026-10-18, tester, full screen):** failed – no hotkey arrived, F10 put the game into menu mode (froze until a click); cause in `docs/re/input.md`. Game otherwise unaffected. Alt+Tab out of full screen and back gives a framed window – also without the mod (tester, same day), so not caused by the mod.
**Result with the `WH_GETMESSAGE` hook (2026-10-18, tester, DE, full screen, with intro):** passed with a gap – the tester pressed Ctrl+Shift+F3 many times (intro, main menu, in game, after Alt+Tab) over two starts: no freeze, game ran normally, exit via the menu and restart fine. `RivellonRenewed.log` only holds the last start: 4× "Test hotkey pressed" on the main thread at 5.4/13.8/25.7/32.7 s (the first during the intro); which press was which situation cannot be told, so "after Alt+Tab" is not proven by the log. Full screen as such is also covered by part A1. Resolution change not tested.

---

## T2.0-W – Debug key events and config options (by the tester, English copy)

Background: `docs/re/key-events.md`, `docs/re/config-files.md`. Everything happens in `C:\dev\dd-test-en` (has no savegames – start a **new game**, never load a real save; rule 7).

**Preparation**
1. Make sure the game is not running. Back up: copy `keylist.txt` → `keylist.txt.bak` and `config.div` → `config.div.bak` in `C:\dev\dd-test-en`.
2. Open `keylist.txt` in Notepad and add these lines **before** the line `# Floating icon keys …` – that section has to stay last (between the columns there is **one Tab**, not spaces; also back up `config.lcl` for part B):
```
-17	- f5	-
-15	- f6	-
-26	- f7	-
-28	- f8	-
-1	- f9	-
```
   (F5 = ClearShroud, F6 = NextHour, F7 = Cheat One Level Up, F8 = Show Buglist, F9 = Drop Console. The editor events −12/−13 are left out on purpose: the editor could write world files.)

**Steps – part A: key events**
1. Start `div.exe` in `C:\dev\dd-test-en`, start a new game (any class), wait until you can walk in the starting area.
2. Press F5, F6, F7, F8, F9 one after another, a few seconds apart. After each key note what happens (map revealed? time of day changes? level up? a window/list? a console?) or "nothing".
3. Input hook after Alt+Tab (open gap of T1.5-W part B): Alt+Tab out of the game and back, then press **Ctrl+Shift+F3** once and note the time.
4. Open the key configuration in the options menu: are F5–F9 listed with names, and which names?
5. Quit the game via the menu (not Alt+F4). Then look at `keylist.txt`: are the five lines still there (the game rewrites the file on exit)?

**Steps – part B: config options** (one change per start; restore `config.div.bak` afterwards)
1. `config.div`: delete the line `no fog` → start, new game, walk outside: is there fog now (compared with before)?
2. `config.lcl`: change `use8bitfont 1` to `use8bitfont 0` → start: do texts look different (dialog with the first NPC, tooltips)? Restore afterwards.
3. `config.lcl`: change `ztunnel 1` to `ztunnel 0` → start, new game: anything different when walking behind walls/buildings? Restore afterwards.

**Expected result:** unknown – this test finds out what the options do. Step A3: a "Test hotkey pressed" line in `RivellonRenewed.log` at the noted time. Screenshots (Steam overlay or Win+Shift+S) of anything interesting help. Afterwards copy the `.bak` files back.

**Result (2026-10-18, tester, EN, full screen, the mod with `LogLevel=debug`):** done – part A not testable, part B answered. `dd-test-en` restored to its original configs afterwards (`savegames\1111` from the test remains).
- Part A: **not testable this way** – `keylist.txt` is only an export; the hand-made F5–F9 bindings were never loaded (no effect) and vanished on exit, and the in-game key configuration does not offer the debug events (`key-events.md`; follow-up in Phase 2, `open-questions.md`). Step A3 (hotkey after Alt+Tab): passed.
- B1 (`no fog` removed): first run unclear (no in-game screenshot); repeat with a screenshot at the same place as in A: fog patches visible → the line switches fog off. B2 (`use8bitfont 0`): text drawn with a Windows serif font instead of the bitmap font (screenshots: dialog, "Door" labels, character sheet), looks sharper. Black map borders in all runs = shroud. B3 (`ztunnel 0`): the see-through effect is off – behind the house the hero is hidden by the roof; with `ztunnel 1` (B1 repeat) the hero shows as a faint silhouette through the roof, the house itself does not become transparent (tester expected more). Roofs vanish when entering a house either way.
- Observation: in earlier starts the screen stayed black at the start of a new game (only the interface) until the first input; in the B1 repeat (fog on) the first room was visible at once (`open-questions.md`). A save made in B2 (`savegames\1111`, test copy only) was loaded in B3.
- Hotkey across the starts: all Ctrl+Shift+F3 presses recognized (5 in B1, 4 in B2 incl. 2 during the first loading screen, delivered late in one burst, 2 in the B1 repeat; none pressed in B3) – except the 2nd start of part A (none, no debug log; `open-questions.md`).

---

## T1.4-W – EXE hash check, scanner and hook manager on Windows (automated)

**Preparation:** build, `ctest -C Release --test-dir build --output-on-failure` (56 checks: SHA-256 NIST vectors, pattern parsing/matching, PE section scan, MinHook create/enable/disable/remove on local functions), then `Install-DdMod -GameDir C:\dev\dd-test`.
**Steps:** `.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test -NoIntro -Seconds 15`
**Expected result:** main menu as usual; `RivellonRenewed.log` reports the verified EXE hash, the code section size and "Hook manager ready".
**Result (2026-10-18, automated, dgVoodoo2 active, 2560×1440 windowed):** passed – unit tests 56/56, main menu as usual, log:
```
[INFO ] EXE hash: CD9F7A07…B42A67 = 1.0062A (Steam) (verified)
[INFO ] Scanner: 2116459 code bytes in 1 executable section(s) of div.exe
[INFO ] Hook manager ready (MinHook)
```
**Result EN (2026-10-18, `C:\dev\dd-test-en`, same setup):** passed – English main menu, dgVoodoo watermark, same log lines (EXE hash verified, same section size).
Hashing `div.exe` (2.4 MB) adds about 50 ms to the start. No game function is hooked yet; the first real hook comes with step 5/6.

---

## T1.1-W – ASI loader + RivellonRenewed.asi load on Windows (automated)

**Preparation:** build (`cmake -S . -B build -A Win32`, `cmake --build build --config Release`), then `Import-Module .\tools\testrun\DdTest.psm1; Install-DdMod -GameDir C:\dev\dd-test`.
**Steps:** `.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test -NoIntro -Seconds 15`
**Expected result:** game reaches the main menu as without the mod; `RivellonRenewed.log` shows version, host `div.exe` 1.0.0.62, platform, config path.
**Result (2026-10-18, automated, dgVoodoo2 active, 2560×1440 windowed):** passed – main menu as usual, log:
```
[INFO ] Rivellon Renewed 0.01 loaded
[INFO ] Host: C:\dev\dd-test\div.exe (file version 1.0.0.62)
[INFO ] Platform: Windows
[INFO ] Config: C:\dev\dd-test\RivellonRenewed.ini
```
Note: "Rivellon Renewed unloading" is missing when the script kills the game (expected); check it after a normal exit.

---

## T0.6-W – dgVoodoo2 with 1.0062A on Windows

**Preparation**
- Folder: `C:\dev\dd-test` (contains `DDraw.dll`, `D3DImm.dll`, `dgVoodooCpl.exe`, `dgVoodoo.conf`).
- Configure `dgVoodooCpl.exe` as described in PLAN.md Phase 0, step 6. **Exception for this first run:** leave "dgVoodoo Watermark" (DirectX tab) **on** – it proves dgVoodoo is active. Turn it off after the test.
- `configtool.exe`: renderer Direct3D, "Use alpha" off, "Smoothness" on. Resolution 1920×1080.

**Steps**
1. Start `div.exe` from `C:\dev\dd-test` (not via Steam).
2. Main menu: note the version shown and whether the dgVoodoo watermark appears (bottom right).
3. Start a new game, walk around for ~2 minutes, open the inventory and a dialog.
4. Look for water: are reflections animated?
5. Save, quit to the menu, load the save.
6. Exit the game. Then set the resolution to 2560×1440 in `config.div` and start again.

**Expected result**
- Watermark visible, version "1.0062A", smooth Direct3D rendering without black graphics / rainbow shadows.
- Save/load works.
- At 2560×1440: crash expected (known limit) – note when exactly (start, menu, in game).

**Result (by the tester, 2026-10-18, full screen, 1920×1080 and 2560×1440)** – passed
- Game, intro and sound work throughout; dgVoodoo watermark visible the whole time; no graphics glitches; water moves and shows reflections (character reflected in a puddle).
- Exiting via the menu works.
- **2560×1440 also works on 1.0062A** – the expected > 1080p crash did not occur on this machine. Only the known scaling issues get worse: intro video, main menu and character creation stay at their fixed size (640×480 / 1024×768, centered), texts and HUD get smaller, more unexplored black area visible.
- Observation: the character only starts **running when the mouse cursor is very far from the character / near the screen edge** – the walk/run distance threshold apparently does not scale with the resolution.
- Observations from the screenshots: HUD bar and dialog box keep their fixed pixel size (centered at the bottom); windows such as the container and the character stats open at fixed positions near the left edge.

---

## T0.7-W – 1.0034A EXE (variant A) on Windows

**Preparation**
- Folder: `C:\dev\dd-test-134` (German Steam data + language-independent files from patch 1.34A + dgVoodoo2; see `docs/re/patch-134.md`).
- Configure `dgVoodooCpl.exe` and `configtool.exe` as in T0.6-W (separate folder → separate settings). Resolution 1920×1080 first.
- Savegames from 1.0062A are not compatible – start a new game only.

**Steps**
1. Start `div.exe` from `C:\dev\dd-test-134`.
2. Main menu: note the version shown (expected "1.0034A"), the language (German?) and whether the menu is centered.
3. Watch the intro / first cutscene: does a CD prompt appear ("Bitte legen Sie die CD …")?
4. Start a new game; check that dialog texts are German and complete (talk to at least two NPCs).
5. Save, quit to the menu, load the save.
6. Exit. Set 2560×1440 in `config.div`, start again, play ~5 minutes (town + one dungeon if possible).

**Expected result**
- Game starts with version "1.0034A", German texts, no crash.
- Possible known issues: menu not centered, CD prompt during cutscenes – note exactly when.
- Resolution above 1080p reportedly works – note the result either way.

**Result (by the tester)**
- _open_

---

## T0.6-D – dgVoodoo2 under Proton on the Steam Deck

Goal: find out early whether dgVoodoo2 (and therefore the D3D11 overlay backend) works under Proton.

**Preparation (Desktop Mode on the Deck)**
- Game folder on the Deck: Steam → Divine Divinity → ⚙ → Manage → Browse local files (usually `~/.local/share/Steam/steamapps/common/divine_divinity`).
- Back up `config.div` and the `slashed*.cfg` files there (e.g. copy them into a folder `_backup`).
- Copy from `C:\dev\dd-test` on Windows (after T0.6-W, with the settings that worked there): `DDraw.dll`, `D3DImm.dll`, `dgVoodoo.conf`, `config.div`, `slashed.cfg`. **Do not** copy `div.exe` or other game files.
- In `dgVoodoo.conf` (text editor), section `[General]`: set `Adapters = all` (the Windows adapter choice is not valid on the Deck). Keep `dgVoodooWatermark = true` in `[DirectX]` for this test.
- In `config.div` (text editor): set `width 1280` and `height 800` (the Deck's native resolution; the copy from `dd-test` contains 2560×1440).
- Steam → game properties → launch options: `WINEDLLOVERRIDES="ddraw,d3dimm=n,b" PROTON_LOG=1 %command%`
- Proton version: default first; if it fails, also try Proton Experimental.

**Steps**
1. Start the game via Steam (Game Mode or Desktop Mode).
2. Check for the dgVoodoo watermark and the version in the main menu.
3. Play ~5 minutes: movement, inventory, a dialog, water reflections.
4. Save and load.
5. Note the FPS / smoothness (Deck performance overlay, level 2+).

**Expected result**
- Watermark visible → dgVoodoo active under Proton (DXVK then handles the D3D11 output).
- If the game crashes or the watermark is missing: send the log `~/steam-214170.log`.

**Undo**
- Delete `DDraw.dll`, `D3DImm.dll`, `dgVoodoo.conf`, restore `config.div`/`slashed*.cfg` from `_backup`, clear the launch options.

**Result (2026-10-18, SteamOS, dgVoodoo2 2.87.5, files copied via SSH, backup `_backup_ddmod_20261018-110251`)** – failed so far
- The override works: Proton loads dgVoodoo's `DDRAW.dll` and `D3DImm.dll` from the game folder ("native"), DXVK creates the D3D11 device on the Deck GPU.
- **Crash right after the D3D11 window is created:** access violation in dgVoodoo's `DDraw.dll` at offset `0x9928` (read near address 0). Reproduced with the game and with `configtool.exe` → "Test".
- Same crash with every variant tried: Proton Experimental 11.0 and Proton 10.0-4; `InheritColorProfileInFullScreenMode` true/false; `config.div` 1280×800 and 1280×720 (the renderer's `slashed-d3d6.cfg` holds its own 1280×720); full screen and windowed. So it is independent of resolution and screen mode.
- One run under Proton 10 played normally (intro, menu, game, sound) – but **without the dgVoodoo watermark**, i.e. with Proton's built-in `ddraw`.
- After the crashes the game reset its configuration although "No" was chosen in the "not correctly terminated" dialog: `config.div` shrank to a minimal version and `slashed-d3d6.cfg` was deleted → "Unable to read configuration file – Exiting". Restored from the backup. Savegames were not affected.
- Next candidates: dgVoodoo2 2.87.3 (oldest version still on GitHub); otherwise play on the Deck without dgVoodoo and use the `DllSlashedEndFrame` overlay fallback there.

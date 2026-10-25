# Engine map of `div.exe`

How much code the game has, how it splits into subsystems and where each one lies – the basis for estimating the "replace subsystems step by step" course (`docs/decisions.md`) and the Phase 11 outlook. Addresses are for the verified 1.0062A build (`game-version.md`); sizes are machine-code bytes as Ghidra 12.1.4 counts them after its default auto-analysis (function bodies, no data).

## Method

- Ghidra headless auto-analysis of `div.exe` and the Larian DLLs (project, scripts and raw output in the scratchpad only, rule 10); an export script lists every function (address, size, callers), the strings each function references, the call graph, imports per function and the RTTI type names.
- **Source-file strings:** `div.exe` holds 200 paths of Larian's source files (`.\AGENTS\agentfight.cpp`, …) in 35 folders. Almost all come from the allocation calls (a debug `new` with file and line) and their exception-cleanup funclets; a few are assert/log messages. Each funclet is mapped back to its parent function (the handler stub after a function's funclets is referenced by that function's prologue), so a string labels the function that allocates.
- **Fill:** MSVC links the object files of one folder back to back, and the folders in **alphabetical order** (`agents` at `0x40EDE0` … `world` at `0x56C060`). A function without a string between two functions of the same file or folder gets that file or folder. Gaps between two folders were named from their strings and imports (table "Identified gaps").
- The DLLs contain no source paths; they are mapped by size, exports and imports only.

## Totals

| What | Value |
|---|---|
| `.text` | `0x401000`–`0x605BFF`, 2 116 608 bytes |
| `.rdata` / `.data` / `.rsrc` | 273 408 / 1 085 636 (virtual) / 6 144 bytes |
| Functions | 10 416, of them 2 263 exception funclets (40 KB), 226 thunks → **7 927 real functions, 1 865 082 bytes** |
| Function size | median 65 bytes, 90 % below 462, 99 % below 1 690; 19 functions over 4 KB, largest 15 399 bytes (`0x41E4E0`, agents) |
| RTTI classes | 599 type descriptors, 568 of them game classes (`CAgent`, `CAgentTactics_*`, `CMagicWizard*Skill_*`, `CProjectile*`, `CBM*Control`, `C*Plate`, Osiris bridge `CDIVINITYOsiris*Function`, …), 31 STL |
| Imports | MSVCR90 (157 symbols, dynamic CRT), KERNEL32, USER32, GDI32 (8), WINMM, IMM32, VERSION, `fmod.dll` (32), `binkw32.dll` (9), `OsirisDLL.dll` and `DivDialogSystem.dll` (C++ classes `COsiris`, `CDivDialogSystem`) |

## Regions of `.text`

| Range | Functions | Bytes | Content |
|---|---:|---:|---|
| `0x401000`–`0x40EDE0` | 146 | 42 804 | root folder: `lightlib.cpp`, credits script parser, **libjpeg 6b** (about `0x403000`–`0x40C000`, decoder), `Main.cpp` (start-up, `0x40C160`) |
| `0x40EDE0`–`0x5A3E90` | 5 865 | **1 518 633** | **game code**, 34 folders in alphabetical order (table below) |
| `0x5A3E90`–`0x5DB000` | 1 609 | 217 748 | **built-in world editor** (`BirdEdit.cpp`, `RegionEdit.cpp`; terrain, object, wall, roof, height, region, location, egg, owner editors; world lock with editor roles and passwords; screenshot capture to PNG) – unused by players |
| `0x5DB000`–`0x5EE260` | 243 | 75 203 | **libpng 1.2.6, zlib 1.2.6**, STL string/container helpers |
| `0x5EE260`–`0x5FE760` | 1 889 | 33 857 | exception funclets and handler stubs (same folder order as the game code) |
| `0x5FE760`–`0x605C00` | 393 | 15 493 | CRT start-up stubs, one large skills function (`0x6012B0`, 11 446 bytes) |

## Subsystems of the game code

Bytes = functions labeled directly or by fill (1 334 229 of 1 518 633 bytes, 88 %). The purpose column is our reading of the file names and strings.

| Folder | Files | Functions | Bytes | Range | Purpose (known mod addresses) |
|---|---:|---:|---:|---|---|
| `agents` | 22 | 676 | 223 887 | `0x40EDE0`–`0x449FE8` | NPCs and monsters: behavior classes, combat tactics, movement, fights, inventory, trade, party, scripts, visualizer; `agentscript.cpp` holds the developer-flag cluster `0x431000`–`0x434000` (`developer-mode.md`) |
| `world` | 20 | 559 | 202 787 | `0x56C060`–`0x5A3E85` | map, cells, tiles, regions, locations, objects, traps, treasure, weather, roofs, book content, map merging, compression |
| `magic` | 16 | 555 | 151 618 | `0x4BDD80`–`0x4E743C` | spells and skill effects, particles, shapes |
| `plate` | 20 | 434 | 104 153 | `0x517FA0`–`0x533DDA` | UI windows ("plates"): trade, inventory, status, options, dialog, book, messages, energy slots, research, chat, bug report (`0x523040`, `0x530AB0` panel sizes) |
| `misc` | 25 | 381 | 93 013 | `0x4EC960`–`0x50564C` | save/load, persist, camera, graphics helpers, floating text, monologues, translator, Targa, Huffman, birds; the two most-called functions of the EXE (`0x4FA540` 969 callers, `0x4FA4B0` 894 – probably memory helpers, not checked) |
| `bmgui` | 6 | 430 | 82 418 | `0x4521C0`–`0x467E06` | GUI control toolkit: buttons, lists, scroll bars, sliders, edit fields, text views (font setup `0x45BF90`, text field `0x4632E0`, `text-rendering.md`) |
| `game` | 7 | 177 | 77 027 | `0x499110`–`0x4AD047` | init, setup, main render loop, mouse, player, Bink playback, editor hooks |
| `diary` | 11 | 250 | 62 059 | `0x474380`–`0x484651` | main menu, character selection, diary, quest log, journal, monster log, statistics |
| `osiris` | 7 | 112 | 41 499 | `0x50C0E0`–`0x516FE2` | bridge to `OsirisDLL.dll`: events, router, NPC/dialog/timer/counter functions |
| `itemstat` | 2 | 116 | 39 564 | `0x4B2360`–`0x4BC7B4` | item statistics, item generation |
| `sound` | 7 | 223 | 39 211 | `0x549AC0`–`0x554072` | sound manager, music entries and regions, reverb, voice playback |
| `weapon` | 2 | 113 | 32 526 | `0x5619B0`–`0x569DD8` | projectiles and arrows |
| `script` | 5 | 163 | 32 054 | `0x5355D0`–`0x53E830` | script interpreter: expressions, statements, sprite lists |
| `skills` | 2 | 94 | 26 422 | `0x541920`–`0x545B80` | skills, thief skills |
| `divevent` | 4 | 86 | 24 766 | `0x485930`–`0x48C2D6` | key events and bindings (`key-events.md`, `0x489D00`) |
| `automap` | 4 | 90 | 20 797 | `0x44BFE0`–`0x4514E6` | map, mini map, user notes |
| `managers` | 7 | 79 | 18 039 | `0x4E7B30`–`0x4EC6EC` | image, animation, index and RAM-file managers |
| `stats` | 1 | 38 | 16 903 | `0x559C60`–`0x55E05F` | character statistics |
| `mplayer` | 2 | 78 | 16 402 | `0x506200`–`0x50A66D` | multiplayer remains (`Message.cpp`, `Server.cpp`) |
| `effect` | 3 | 37 | 10 669 | `0x490240`–`0x49329A` | screen effects, filters, fire |
| `invent` | 1 | 30 | 9 781 | `0x4AE420`–`0x4B0CC4` | inventory model |
| 13 small folders | 14 | 73 | 20 080 | | `npc` (combine), `unicode` (`0x55F030`), `scanner`, `font` (`0x493A30`), `divshell` (settings `0x48D130`), `party`, `cube`, `sortspr`, `savegame`, `cluebook`, `dialog` (calls `DivDialogSystem`), `global`, `credits` |

**Grouped (including the identified gaps below):** gameplay (`agents`, `world`, `magic`, `weapon`, `skills`, `itemstat`, `stats`, `invent`, `npc`, `party`) about 730 KB; UI (`plate`, `bmgui`, `diary`, `automap`, `cluebook`, `font`) about 300 KB; engine base (`game`, `misc`, `managers`, `effect`, `sortspr`, sprite path) about 220 KB; story and scripts (`osiris`, `script`, `scanner`, `divevent`) about 115 KB; sound about 50 KB.

### Identified gaps (no source string, named by strings and position)

| Range | Bytes | Between | Content |
|---|---:|---|---|
| `0x467E10`–`0x46EFAB` | 27 104 | `bmgui` / `cluebook` | STL iostream and locale code |
| `0x554080`–`0x559C4F` | 22 483 | `sound` / `stats` | probably folder `spranim` (`path.cpp` string): renderer DLL binding (`slash%d.dll`, `DllSlashedInit`), sprite span blitting (`0x556A80`, `0x5587F0`, `0x559750`) |
| `0x494440`–`0x499104` | 18 715 | `font` / `game` | rest of `font`: `.fnt` loading (`0x4958C0`), font list, `ttfonts.lst`, installed-font enumeration (`0x499040`) |
| `0x5468A0`–`0x549AB2` | 11 954 | `sortspr` / `sound` | FMOD wrapper, `sound.cfg`, `snd_override.txt` |
| `0x53E830`–`0x541919` | 10 958 | `script` / `skills` | script parser messages, skill constants |
| `0x55F4F0`–`0x5619B0`, `0x569DE0`–`0x56C060` | 17 877 | around `weapon` | projectile and arrow type tables |
| `0x48D6F0`–`0x490237` | 8 819 | `divshell` / `effect` | config reading (`config-files.md`), map and record files |
| `0x449FF0`–`0x44BFDB` | 7 188 | `agents` / `automap` | automap user notes, zoom buttons |
| `0x516FF0`–`0x5178AF` | 2 155 | `osiris` / `party` | story loading and story patching (`story.000`, `story_merge_*.dat`) |

Of the 184 KB still unlabeled in the game code these cover about 127 KB; the rest are small gaps at folder borders.

## Dependencies between subsystems

From Ghidra's call graph: 21 966 direct calls between functions (funclets counted for their parent); **virtual calls are not included**, so links through base classes (UI controls, skills, agents) are under-counted. Named gaps count for their folder (`font`, `sound`, `spranim`, …; "STL" = the iostream gap at `0x467E10`).

**Shared helpers:** 136 functions are called from five or more other folders – small memory and string helpers (`0x4FA4B0` from 39 folders / 894 callers, `0x4FA540` 960 callers, both in `misc`), CRT/STL stubs in the library region (`0x5E5BDA` 471 callers), list and container helpers in `world` and `agents`, image lookups in `managers`. They are left out of the edges below; a replacement may call them like any other code.

**Layers (calls without the helpers):**

| Layer | Folders | Shape |
|---|---|---|
| Top | `editor`, `game` | `editor` calls everything (1 220 calls out, 34 in); `game` (main loop) calls `world` 139, `misc` 77, `agents` 56, `plate` 47, `magic` 36 |
| UI | `plate`, `diary`, `automap`, `skills` panel | `plate` → `bmgui` 55, `game` 27, `agents` 20, `world` 17; called by `game` 47, `misc` 26, `diary` 17 – **85 entry functions** used from outside |
| Gameplay core | `agents` ↔ `world` (103 / 42 calls), `magic` ↔ `agents` (131 / 21), `magic` ↔ `script` (134 / 16), `osiris` ↔ `agents` (48 / 37), `weapon` → `magic` 72 | tightly coupled in both directions; `agents` has 197 and `world` 247 entry functions used from outside |
| Services | `bmgui` (→ `scanner`, `font`, `divevent`), `sound`, `managers`, `spranim`, `font`, `unicode`, `scanner`, `stats`, `itemstat`, `invent` | mostly called, few calls out; `font`: 22 entry functions, called by `bmgui` 13 and `plate` 7 |
| Libraries | libjpeg, libpng/zlib, STL | called only; `divevent` writes `keylist.txt` through iostream |

`tail` (`0x5FE760`–) is called by nobody directly – static initializers of global tables (projectile types, skills) run by the CRT at start. `editor` ↔ `world` (203 / 6) shows the editor is built on top of the game and is not needed by it.

**For replacing subsystems:** the UI block has a narrow, mostly one-way interface (game → UI, UI → `bmgui` toolkit); fonts are a service with 22 entry points; the gameplay core (`agents`, `world`, `magic`, `script`, `osiris`) is one tightly coupled cluster – changes there stay targeted (single functions or tables), not block replacements.

Class hierarchies (RTTI) and the Larian DLLs: `engine-classes.md`.

## What this means for the course

- **Size of Larian's own runtime code:** about 1.5 MB machine code in about 5 900 functions in `div.exe` (without editor, libraries and funclets), plus about 0.3 MB in Osiris, the dialog system and one renderer (without CRT). As a rough order of magnitude for optimized x86 at 8–15 bytes per source line: 100 000–200 000 lines of C++ – an estimate, not measured.
- **Clear seams:** each folder is one contiguous block, so a subsystem's code, its entry points (calls from other blocks) and its size can be listed before work starts. The renderer is already behind a DLL interface (15 exports), the story engine and dialogs too – the cheapest places to put our own implementation behind the original interface.
- **Sizes of candidates:** font code about 21 KB (`font` + `0x494440` gap, plus the setup in `bmgui`) – a good REA trial size; sound about 50 KB; UI about 300 KB (many small windows, each replaceable on its own); gameplay about 730 KB – only targeted changes, no rebuild.
- **The editor (218 KB) and multiplayer remains (16 KB) can be ignored** for every player-facing feature.
- **About 55 % of the game code ran in 45 minutes of real play plus the scripted runs** (runtime share below) – the part a replacement must reproduce is smaller than the static size, though every subsystem keeps rare paths (spells, ranged combat, later chapters) that these sessions miss.

## Runtime share

Measured with `ddmod_coverage.asi` (`tools/coverage/`, developer tool): a one-shot breakpoint on each of the 8 152 non-funclet function entries, the first hit recorded. Two sets of runs:

- **Scripted runs** in `dd-test` (DE, windowed 2560×1440, without `RivellonRenewed.asi`, from process start): (1) main menu → load a save → inventory, letter, book, diary, map, walk out, save dialog; (2) NPC dialog with answers; (3) options pages; (4) credits; (5) new game → character selection.
- **Play sessions** of the human in `dd-play` (TRE.1-W, `docs/test-results.md`; with the mod, `defer-window`, so start-up code before the game window is missed): new game, walking, NPCs, traders, chests, letters/books, cut-scene, cellar, melee fights with a skeleton and rats, intro and credits from the menu, picture settings – about 45 minutes in three starts. No ranged weapon, no spell, no level-up.

| Measure | Value |
|---|---|
| Start to main menu | 1 583 functions |
| Five scripted runs | game code 2 313 of 5 865 functions (39 %), 592 of 1 519 KB (39 %) |
| Three play sessions | game code 3 091 functions (53 %), 853 KB (56 %) |
| **All eight runs** | 3 653 functions; game code **3 220 of 5 865 functions (55 %), 877 of 1 519 KB (58 %)** |

Share of bytes run (all eight), by folder: `itemstat` 92 %, `automap` 87 %, `diary` 86 %, `sound` 85 %, `managers` 79 %, `script` 77 %, `effect` 76 %, `osiris` 75 %, `stats` 65 %, `plate` 61 %, `game` 59 %, `agents` 58 %, `world` 58 %, `bmgui` 58 %, `misc` 57 %, `skills` 29 %, `magic` 21 %, `weapon` 15 % (the last three are spells and ranged combat, barely exercised). The editor ran 3 % of its bytes (set-up code), libpng/zlib 5 %.

**Font code:** of the 90 functions in `0x493A30`–`0x499104` only about 20–30 ran per run – the bitmap-font path (`use8bitfont 1`); most of the rest is the GDI font path and the font list (`text-rendering.md`).

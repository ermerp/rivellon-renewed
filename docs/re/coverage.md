# Runtime coverage of `div.exe`

Which share of the game code actually runs in play – part of the engine map ([engine-map.md](engine-map.md), whose folder names and sizes this file uses). Addresses are for the verified 1.0062A build (`game-version.md`). Tool usage: [research-tools.md](../dev/research-tools.md).

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

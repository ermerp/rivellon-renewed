# Configuration files and the settings object

## Files

- `config.lcl` (installation/language) and `config.div` (display/user settings): plain text, one `key value` per line, `//` comments. **The game rewrites `config.div` on exit** (values kept); `configtool.exe` edits it too and resets "alpha bit" every time it opens. The resolution can only be changed with `configtool.exe` before the start – the game has no in-game resolution option (tester).
- `slashed.cfg` (4 bytes): selected renderer = first DWORD (`0` Direct3D 6, `1` Glide, `2` DirectDraw, `3` Software). Each `slashed-<name>.cfg` starts with its renderer ID and holds a resolution pair at offsets `0x0C`/`0x10` (full `slashed-d3d6.cfg` layout: `renderer-d3d6.md`). Which one decides the resolution: "Resolution at startup" below.
- **Fresh Steam install** (DE, checked 2026-10-25 before the first start): ships `slashed.cfg` (renderer 0, Direct3D 6), `slashed-d3d6.cfg` (800×600, full screen), `slashed-directx.cfg`, `slashed-software.cfg`, `config.div` (1024×768, `smooth 1`, `no fog`) and `config.lcl` (800×600, `language 2`), so `div.exe` starts without `configtool.exe`. Steam asks at every start "Divine Divinity spielen" / "Configure" (two launch options), and its `installscript.vdf` runs `configtool.exe` ("Slashed Configuration v1.2") once at the first start: Direct 3D and the `config.div` size (1024×768) preselected, Smooth on, AlphaBit off; "Apply & Close" unchanged leaves `config.div` as it is, then the game starts (afterwards `slashed-d3d6.cfg` holds 1024×768 – the game's shutdown write-back). Steam treats the existence of `HKLM\SOFTWARE\WOW6432Node\Valve\Steam\Apps\214170` as "already run", and uninstalling does not remove that key (nor the files the game wrote: `dynamic\`, `init.cfg`, `sinfo.000`). Untouched copy of this state: `C:\dev\dd-steam-clean`.
- **Write-back of `config.div`** (`0x48C6F0`, `__thiscall` on the settings object, no arguments): runs on exit **and when an options page is applied** ("Übernehmen"/"Apply"). Each key is written only if its keyword index is found in a set at settings `+0x154`. Observed: only the keys that were in `config.div` come back (the working copies keep their 5 keys), keys from `config.lcl` such as `use8bitfont` never appear. Where the set is filled is not traced (`open-questions.md`). Fog: written as `// disables fog rendering` plus `no fog` (fog off) or `//no fog` (fog on) – only if the line was in the file; without it the picture settings' fog choice is lost at the next start (in the unmodded game too).
- **Run markers** `init.cfg` (4 bytes) and `sinfo.000` (8 bytes), created by the game. After a kill: `init.cfg = 00 00 00 00`, `sinfo.000 = 01 00 00 00 01 00 00 00`; after a clean exit: `02 00 00 00` / `01 00 00 00 02 00 00 00`. After an unclean exit the next start asks "Divinity was not correctly terminated … reset the configuration?" – never answer "Yes".
- **Self-reset after repeated crashes** (seen on the Deck, even with "No"): `config.div` rewritten to a minimal default, `slashed-d3d6.cfg` deleted; the next start fails with "Unable to read configuration file" and opens `configtool.exe`. Savegames untouched. Keep backups of `config.div` and `slashed*.cfg` for crash tests.
- Also written at runtime: `persist.dat`, `keylist.txt` (`key-events.md`), `mapids.000`, `dynamic/*`, `dat/skillcosts.txt`, `dat/usernotes.bin`.

## Resolution at startup

Traced in the disassembly of 1.0062A (step 7.2); addresses in `div.exe`.

1. **Settings** (`0x40C160`, early in the start-up): creates the settings object, runs the reader on `config.lcl` (or a file named on the command line), then on `config.div`. After each reader call it copies settings `+0x38`/`+0x3C` (`width`/`height`) into the game's **screen size globals `0x64D9B8`/`0x64D9BC`** – the values the whole game uses (more than a hundred readers: layout, clipping, movement). Settings `+0x38`/`+0x3C` themselves are read nowhere else (only by the write-back of `config.div`).
2. **Renderer start** (`0x49FF20`): opens `slashed.cfg` (missing → "Configuration file not found. Please run configuration tool first. Exiting."), reads the renderer ID, loads `slash<ID>.dll` and calls `DllSlashedInit` (`0x557C50`) – which sets the mode stored in `slashed-d3d6.cfg` (its own fallback: 640×480).
3. **Right after that** it fetches the renderer's current configuration block (`DllSlashedGetInfo(5)`), replaces width/height with the screen size globals and gamma with settings `+0x98`, and hands it to `DllSlashedInternalApplyConfiguration` (through `0x557D00`). Failure is fatal: "Can't change resolution", exit.

So **`config.div` decides the resolution**; `slashed-d3d6.cfg` only sets the short first mode during `DllSlashedInit`, and `DllSlashedShutdown` writes the final size back into it. In full screen the size must be in the renderer's mode list (`renderer-d3d6.md`), otherwise step 3 ends the game; windowed takes any size.

Later mode changes go through the same `0x557D00` with a copied block and the screen size globals: the video player switches to 640×480 for a movie and back (`0x40D1E0`), and the `resolution switch` path toggles between 640×480/800×600 and the globals (`0x40CB41`).

## Parser and settings object

- Reader `0x48D6F0` (`__thiscall` on the settings object, one argument: the file name, `ret 4`) reads one file line by line; it is called for `config.lcl`, then `config.div`; both go through the keyword parser `0x48D130`. It tries 79 keyword patterns from a pointer table at `0x60FD00` (`#` = number, `$` = string) and stores values via a `switch` into the **settings object**.
- Settings object: `0x174` bytes, created at startup in `0x40C160` (constructor `0x48D7D0` sets the defaults), pointer in the global `0x0065B3D0`.

### Stored keys

| Key | Field | Notes |
|---|---|---|
| `rgb 555` / `rgb 565` | `+0x24` = 0 / 1 | 16-bit pixel format |
| `width #` / `height #` | `+0x38` / `+0x3C` | |
| `memory limit #` | `+0x68` | |
| `ztunnel #` | `+0x88` | see-through effect: with `1` (default) the hero stays visible behind buildings as a faint silhouette drawn through the roof – the building itself does not become transparent; with `0` the hero disappears behind the house (T2.0-W B3 and B1 repeat, EN, tester screenshots). Removing the roof when the hero is inside a house is separate and works with both values. Idea for Phase 6/8: a stronger, configurable see-through |
| `gamma $` / `contrast $` | `+0x94` / `+0x98` (float) | |
| `no fog` | `+0xA4` (dword) = 1 | Steam `config.div` (DE and EN) already contains it → fog off by default. Fog on: whitish ground mist patches (save `T-Nebel`, T1.18-W). The black borders are the shroud, not fog. Switched in game under Options → (first entry) → "Leistung"/performance, "NEBEL"/"FOG" (with quality, transparency/"ztunnel", "3D shadows" – the shadow key is not identified); the page reads the flag when it opens and the game uses it from the next frame. The mod sets it after each reader call (`[Features] Fog`, `src/features/fog.cpp`) |
| `no warning` | `+0xA8` = 0 | |
| `images compressed` | `+0xB8` = 1 | |
| `flat files` | `+0xC0` = 1 | **required** – without it: "static\light.lib : file not found" |
| `startup logo #` | `+0xCC` | intro movies |
| `alpha bit #` | `+0xF4` | "Use alpha" in `configtool.exe` |
| `quickstart $` | `+0x118` (string) | **required** – without it `div.exe` exits immediately (exit code 0, no window) |
| `quicksave $` | `+0x11C` (string) | |
| `use8bitfont #` | `+0x120` (byte) | read only by the font setup (the mod's font mode forces it to 0 there, `fonts-mod.md`). `0` (default `1`): the game draws text with a Windows (GDI) serif font instead of its own bitmap font – dialogs, tooltips ("Door" instead of the small-caps "DOOR"), character sheet; looks sharper at 2560×1440 (T2.0-W B2, EN, tester screenshots). Fits the `CreateFontA`/`GetGlyphOutlineA` imports (`architecture.md`). Candidate for Phase 6 (readability) |
| `language #` | `+0x124` | 1 = English, 2 = German |

Further stored keys: `game rate # fps`, `sound #`, `smooth #`, `explain #`, `resolution switch #`, `fog speed #`, `voice comments #`, `force mac id $`, `compress reports #`, `queue sounds #`, `patching #`, `roofs #`, `SplotchProblem #`, `rttm on`, `path $`, `exe name $`, `exe path $`.

### Recognized but ignored in 1.0062A

Parsed, nothing stored (switch default): `debug on`, `strong debug`, `slashdebug #`, `multiplayer`, `server`, `client`, `connection #`, `session $`, `max players #`, `guaranteed messaging`, `update speed #`, `line of sight on/off`, `direct move #`, `direct input #`, `dictionary $`, `role # user $`, `exhibition #`, `no decompress`, `egg editing`, `map merge`, `egg merge`, `compile start`, `memory log startup/game`, `generate all eggs`, `merge inventories`, `commit to disk #`, `startthrough #`, `force compiled roofs #`, `arty mode`, `gforcefix #`, `MaxQuickSaveAmount #`, `game $`, `map mode #`, `mapgen …`, `macro …`, `pathfinding power #`, `set monster generation #`, `convert eol`. Removing `debug on` from the English `config.lcl` changes nothing (start test).

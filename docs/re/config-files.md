# Configuration files and the settings object

## Files

- `config.lcl` (installation/language) and `config.div` (display/user settings): plain text, one `key value` per line, `//` comments. **The game rewrites `config.div` on exit** (values kept); `configtool.exe` edits it too and resets "alpha bit" every time it opens.
- `slashed.cfg` (4 bytes): selected renderer = first DWORD (`0` Direct3D 6, `1` Glide, `2` DirectDraw, `3` Software). Each `slashed-<name>.cfg` starts with its renderer ID and holds a resolution pair at offsets `0x0C`/`0x10`. On the PC `config.div` decided the resolution; on the Deck with dgVoodoo the value from `slashed-d3d6.cfg` won – keep both in sync.
- **Run markers** `init.cfg` (4 bytes) and `sinfo.000` (8 bytes), created by the game. After a kill: `init.cfg = 00 00 00 00`, `sinfo.000 = 01 00 00 00 01 00 00 00`; after a clean exit: `02 00 00 00` / `01 00 00 00 02 00 00 00`. After an unclean exit the next start asks "Divinity was not correctly terminated … reset the configuration?" – never answer "Yes".
- **Self-reset after repeated crashes** (seen on the Deck, even with "No"): `config.div` rewritten to a minimal default, `slashed-d3d6.cfg` deleted; the next start fails with "Unable to read configuration file" and opens `configtool.exe`. Savegames untouched. Keep backups of `config.div` and `slashed*.cfg` for crash tests.
- Also written at runtime: `persist.dat`, `keylist.txt` (`key-events.md`), `mapids.000`, `dynamic/*`, `dat/skillcosts.txt`, `dat/usernotes.bin`.

## Parser and settings object

- Loader `0x48D6F0` reads `config.lcl`, then `config.div`; both go through the keyword parser `0x48D130`. It tries 79 keyword patterns from a pointer table at `0x60FD00` (`#` = number, `$` = string) and stores values via a `switch` into the **settings object**.
- Settings object: `0x174` bytes, created at startup in `0x40C160` (constructor `0x48D7D0` sets the defaults), pointer in the global `0x0065B3D0`.

### Stored keys

| Key | Field | Notes |
|---|---|---|
| `rgb 555` / `rgb 565` | `+0x24` = 0 / 1 | 16-bit pixel format |
| `width #` / `height #` | `+0x38` / `+0x3C` | |
| `memory limit #` | `+0x68` | |
| `ztunnel #` | `+0x88` | effect unknown (T2.0-W) |
| `gamma $` / `contrast $` | `+0x94` / `+0x98` (float) | |
| `no fog` | `+0xA4` = 1 | Steam `config.div` (DE and EN) already contains it → fog off by default; effect of removing it: T2.0-W |
| `no warning` | `+0xA8` = 0 | |
| `images compressed` | `+0xB8` = 1 | |
| `flat files` | `+0xC0` = 1 | **required** – without it: "static\light.lib : file not found" |
| `startup logo #` | `+0xCC` | intro movies |
| `alpha bit #` | `+0xF4` | "Use alpha" in `configtool.exe` |
| `quickstart $` | `+0x118` (string) | **required** – without it `div.exe` exits immediately (exit code 0, no window) |
| `quicksave $` | `+0x11C` (string) | |
| `use8bitfont #` | `+0x120` (byte) | effect unknown (T2.0-W) |
| `language #` | `+0x124` | 1 = English, 2 = German |

Further stored keys: `game rate # fps`, `sound #`, `smooth #`, `explain #`, `resolution switch #`, `fog speed #`, `voice comments #`, `force mac id $`, `compress reports #`, `queue sounds #`, `patching #`, `roofs #`, `SplotchProblem #`, `rttm on`, `path $`, `exe name $`, `exe path $`.

### Recognized but ignored in 1.0062A

Parsed, nothing stored (switch default): `debug on`, `strong debug`, `slashdebug #`, `multiplayer`, `server`, `client`, `connection #`, `session $`, `max players #`, `guaranteed messaging`, `update speed #`, `line of sight on/off`, `direct move #`, `direct input #`, `dictionary $`, `role # user $`, `exhibition #`, `no decompress`, `egg editing`, `map merge`, `egg merge`, `compile start`, `memory log startup/game`, `generate all eggs`, `merge inventories`, `commit to disk #`, `startthrough #`, `force compiled roofs #`, `arty mode`, `gforcefix #`, `MaxQuickSaveAmount #`, `game $`, `map mode #`, `mapgen …`, `macro …`, `pathfinding power #`, `set monster generation #`, `convert eol`. Removing `debug on` from the English `config.lcl` changes nothing (start test).

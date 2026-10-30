# Steam Deck access (SSH)

The Deck is usually off – announce beforehand when a task needs it and wait until the human has switched it on and enabled SSH (`CLAUDE.md`). Input sequences: [test-sequences.md](test-sequences.md).

## Connection

- `ssh deck@<deck-ip>` (the real IP is in Claude's local memory, not in the repo – else ask the human; key auth, `~/.ssh/id_ed25519`); always `-o BatchMode=yes -o ConnectTimeout=15`. Unreachable → off or asleep, ask the human.
- Run remote scripts from the **Bash tool** with a heredoc: `ssh -o BatchMode=yes deck@<deck-ip> 'bash -s' <<'EOF' … EOF`. Not from PowerShell 5.1 (strips inner quotes); no files with CRLF/BOM.
- `pgrep -f div.exe` matches itself – use `pgrep -x div.exe`.

## Game and files

- Game folder: `~/.local/share/Steam/steamapps/common/divine_divinity` (German, same `div.exe` as on Windows; 1280×720 in `config.div` and `slashed-d3d6.cfg` – PC-vs-Deck comparisons run at the same resolution on both). Savegames in `savegames/`.
- Launch options are set by the human in Steam (Game Mode); hand them over as a text file on `~/Desktop`. For the mod: `WINEDLLOVERRIDES="winmm=n,b" %command%`; `PROTON_LOG=1` writes `~/steam-214170.log` (overwritten on every start).
- Steam screenshots (Steam+R1): `~/.local/share/Steam/userdata/*/760/remote/214170/screenshots/`.
- Backups of the Deck's savegames and game-folder backups on the PC: `C:\dev\backups\deck-divine_divinity-<YYYYMMDD-HHMMSS>`; `deck.sh install` also keeps `_backup_rr_<time>` in the game folder.
- **First start after a (re)install:** Steam runs the game's install script, then `configtool.exe` ("Slashed Configuration", display `:1`; "Apply & Close" deletes `slashed-d3d6.cfg`, the game writes it again on exit) – and that first `div.exe` runs **without the launch options** (no mod): quit it from the menu, the next start has them. In Game Mode Steam started that next run by itself right after the quit (seen once, T1.38-D).

## `tools/testrun/deck.sh`

`deck.sh <user@host> check|install|testsaves|start|steps|fetch|uninstall` (usage at its top):
- `check` (read only) first: no `div.exe` running, no foreign files in the game folder, renderer `slashed.cfg` = 0 (Direct3D 6 – the overlay draws only through `slash1.dll`), resolution. Anything unexpected: stop and tell the human.
- `install [--loglevel debug|info]` backs up saves and configs, installs, writes the launch-options file; `testsaves <names>` copies `T-*` saves that are not on the Deck yet.
- `start` starts the game through Steam and waits for the overlay's first frame; `steps [--out <dir>] <steps>` sends input with `xdotool` on the game's Xwayland display and takes screenshots with `gamescopectl screenshot` (the game's own surface, no Steam UI) – **Game Mode only**, and a shot only while gamescope reports app 214170 as focused (rule 11). Steps in `tools/testrun/deck-steps.sh`.
- `fetch` copies logs and the Proton log to `C:\dev\screenshots\deck\`; `uninstall` removes the mod (then clear the launch options).
- Switching to Desktop Mode ends a running game; the human must not touch the Deck during a run.
- Over non-game windows `xdotool mousemove --window` does not move the pointer – use `mousemove_relative`. A click must hold the button for a moment. Deck shots can lag one step: wait 3–4 s before a `shot`.
- After a round: `install --loglevel info` (keeps the backup step).

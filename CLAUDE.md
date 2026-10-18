# CLAUDE.md – dd-modern

Modernization mod for Divine Divinity (Steam/GOG 1.0062A) as an `.asi` plugin loaded by Ultimate ASI Loader. Plan and status: [PLAN.md](PLAN.md). Work through the phases in order.

## Start of a session (Session-Start, `/catchup`)

- Plan file = `PLAN.md`; findings = `docs/re/` (there is no `findings.md`).
- Read **only** this file and the sections "Current status", "Decisions" and "Rejected approaches" in `PLAN.md`, plus `git log --oneline -10` and `git status`. Everything else (rest of `PLAN.md`, `docs/re/*`, `docs/testing.md`) only when the task needs it – start from `docs/re/README.md` (one line per topic file).

## Session wrap-up (Session-Abschluss, `/wrapup`)

- Plan file = `PLAN.md`: update "Current status" (stage done/partial/blocked, next concrete step, what waits for the human), "Decisions" (one line each, with reason) and "Rejected approaches" (one line each, why).
- Findings = `docs/re/<topic>.md`: update the matching topic file, add a line to `docs/re/README.md` for a new file, keep `docs/re/open-questions.md` current. Test results go to `docs/testing.md`.
- Follow the documentation rules below (update, don't append; delete what is outdated).
- Propose a commit message (English, with the attribution line) and **commit only after the human confirms** – this overrides the general "Claude commits doc changes".

## Language

- **Talk to the human in German.**
- **Everything in the repo is English:** code comments, commit messages, README, `docs/`, `CLAUDE.md`, `PLAN.md`, `ddmod.ini` comments, log output.

## Fixed rules

1. **No Larian files in the repo** (EXE, DLLs, graphics, texts, audio, fonts, screenshots). Maintain `.gitignore`.
2. **No decompiled code / pseudocode in the repo** – not even reworded by an AI. Document findings in our own words in `docs/re/` (purpose, signature, address, structure layout). The implementation is always our own code.
3. **No binary patches of the EXE.** Everything goes through the plugin.
4. **Every feature can be switched off individually** via `ddmod.ini`. Invasive features (fast travel, balancing) are off by default.
5. **Generate derived content only locally on the user's machine** (upscaling, TTS). The mod ships tools, not data.
6. **No cloned voices of real voice actors** – only generic synthetic voices.
7. **Protect savegames:** back up saves before tests that write game state; test state-changing features with throwaway saves first.
8. **No hard-coded addresses** – use signature scans; fixed addresses only as a documented fallback for the EXE verified by SHA-256.
9. **Ask instead of guessing:** for in-game tests, write clear test instructions (what to do, what to observe) in `docs/testing.md` and wait for the result.
10. **Analyze only the game's own files** (EXE/DLLs). Do not disassemble third-party tools (dgVoodoo2, ASI loader). Disassembly output goes to the scratchpad only.
11. **Screenshots only of the game window, never of the desktop.** If no game window exists (game exited, not started, only another program's dialog), take no screenshot – skip it and report that. The desktop can show private data. Never open desktop-sized images produced by our own scripts or tools. **Exception – the human's captures:** when the human writes that they took screenshots, Claude may open and evaluate images from `%USERPROFILE%\Videos\Captures` regardless of size (the folder is then cleaned up and released). Only images from the test period in question whose file name names the game window (`Divinity …`); skip anything else. Never copy these images into the repo.

## Documentation rules

- **Update instead of append:** change the existing statement; delete what is outdated instead of correcting it with a note. No history or dates of past steps – git has them.
- **One topic per file:** RE findings in `docs/re/<topic>.md` (list in `docs/re/README.md`), byte layouts in `docs/re/file-formats.md`, open questions in `docs/re/open-questions.md` (remove answered ones), test instructions and results in `docs/testing.md`.
- `CLAUDE.md`: only rules, paths and workflows (max. ~150 lines) – no findings, no status. `PLAN.md` "Current status": short (where we are, next steps).

## Paths (dev PC, Windows 11)

| What | Path |
|---|---|
| Repo | `C:\dev\dd-modern` |
| Original game (Steam, German, 1.0062A) | `C:\Program Files (x86)\Steam\steamapps\common\divine_divinity` – **read only, never modify** |
| Working copy German (1.0062A + dgVoodoo2 2.87.5 + ddmod) | `C:\dev\dd-test` – 2560×1440 (keep), originals of changed configs as `*.orig` |
| Working copy English (same setup) | `C:\dev\dd-test-en` – 2560×1440, originals as `*.orig` |
| Reference copy 1.0034A (on hold, do not change) | `C:\dev\dd-test-134` |
| Downloads (verified, outside repo) | `C:\dev\downloads` – `patch134_en\files`, `dgvoodoo2_87_5`, `dgvoodoo2_87_3`, `ultimate-asi-loader_9.7.4\dinput8.dll` |
| Screenshots / fetched logs (never commit) | `C:\dev\screenshots` (Deck files in `deck\`) |
| Ghidra 12.1.4 | `C:\tools\ghidra_12.1.4_PUBLIC` (JDK: Temurin 21) |
| Cheat Engine 7.7 | `C:\Program Files\Cheat Engine` |
| x32dbg | `x32dbg` (winget alias) |
| MSVC | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` (`dumpbin`: `VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\dumpbin.exe`) |
| 7-Zip | `C:\Program Files\7-Zip\7z.exe` |

Hardware: GTX 1050 Ti (DXGI adapter 1, primary monitor 2560×1440) + Intel UHD 630 (adapter 2, second monitor).

## Working agreements with the human

- Ask before installing software (winget) and before any change on the Steam Deck; show the commands first.
- Downloads only from official sources, verify SHA-256 against the published digest; scripts must abort if no unique asset is found.
- The human keeps `PLAN.md` decisions; Claude maintains `PLAN.md` (status, results) and commits doc changes.
- **Context handover:** after every completed phase, a larger sub-step or a longer debugging session, suggest `/wrapup`, then `/clear`, then `/catchup` (optionally with the task as argument, e.g. `/catchup Phase 1 step 5`).

## Build

- The game is **32-bit** → always build x86.
- `cmake -S . -B build -G "Visual Studio 17 2022" -A Win32`, then `cmake --build build --config Release` → `build/Release/ddmod.asi` (static CRT, `/W4` clean). Unit tests (no game needed): `ctest -C Release --test-dir build --output-on-failure`.
- One feature per branch. Current branch: see `PLAN.md` "Current status".

## Test workflow (Windows)

PowerShell module `tools/testrun/DdTest.psm1` (refuses the Steam folder):
- `Install-DdMod -GameDir <copy> [-ResetIni]` – copies the ASI loader as `winmm.dll` (never replaces a foreign `winmm.dll`), `ddmod.asi`, `ddmod.ini` (kept if present).
- `Uninstall-DdMod -GameDir <copy>` – removes `winmm.dll` (only if it is the loader), `ddmod.asi`, `ddmod.log`.
- `Set-DgVoodooProfile -GameDir … -Adapter 1 [-Watermark]`, `Set-DivConfig -GameDir … -Width … -Height …`.

`tools/testrun/Start-GameTest.ps1 -GameDir <copy> [-NoIntro] [-PressEscapeAt 4,8,…] -Seconds 15 -Count 2 -Interval 10`: starts the game windowed (temporarily `FullScreenMode=false`), screenshots of the game window to `C:\dev\screenshots`, closes the game, restores configs and the run markers `init.cfg`/`sinfo.000`, answers the "not correctly terminated" dialog with **No**, prints `ddmod.log`. View screenshots with the Read tool. Crash details: Windows event log, Application, ID 1000.

**Every phase is tested in both working copies** (`dd-test` German, `dd-test-en` English): `Install-DdMod` + `Start-GameTest.ps1` for each; record both results in `docs/testing.md`.

Pitfalls:
- The game treats a kill as a crash (run markers); **never answer "Yes"** to the reset dialog.
- After repeated crashes the game may reset `config.div` and delete `slashed-d3d6.cfg` → restore from backup. Back up any config file before changing it in a test.
- The game rewrites `config.div` on exit. Resolution lives in `config.div` **and** `slashed-d3d6.cfg` (offsets 0x0C/0x10).
- `config.lcl` needs `flat files` and `quickstart` – without them the game does not start.
- In heredocs of the Bash tool, backslash escapes in inline Python can get mangled – write scripts with the Write tool instead.

## Steam Deck access (SSH)

- **The Deck is usually off.** Never assume it is reachable: announce beforehand when a task needs it, and wait until the human has switched it on and enabled SSH. No connection attempts without that announcement.
- `ssh deck@<deck-ip>` (key auth, `~/.ssh/id_ed25519`); always `-o BatchMode=yes -o ConnectTimeout=15`. Unreachable → off or asleep, ask the human.
- Run remote scripts from the **Bash tool** with a heredoc: `ssh -o BatchMode=yes deck@<deck-ip> 'bash -s' <<'EOF' … EOF`. Not from PowerShell 5.1 (strips inner quotes); no files with CRLF/BOM.
- `pgrep -f div.exe` matches itself – use `pgrep -x div.exe`.
- Game folder: `~/.local/share/Steam/steamapps/common/divine_divinity` (German, same `div.exe`; own `config.div` 1280×720, `rgb 565`). Savegames in `savegames/`.
- Backups: `divine_divinity/_backup_ddmod_20261018-110251` (original configs, run markers, all savegames); disabled dgVoodoo files in `divine_divinity/_ddmod_dgvoodoo_disabled`; `config.div.broken` = config after a self-reset.
- Proton log (launch option `PROTON_LOG=1`): `~/steam-214170.log`, overwritten on every start. Steam screenshots (Steam+R1): `~/.local/share/Steam/userdata/*/760/remote/214170/screenshots/`.
- Launch options are set by the human in Steam (Game Mode); hand them over as a text file on `~/Desktop`. For the mod: `WINEDLLOVERRIDES="winmm=n,b" %command%`.

## Workflow per feature

Investigate → document in `docs/re/` → implement minimally behind a feature flag → test instructions in `docs/testing.md` → expand and harden → commit. Test every graphics/overlay change on PC **and** Deck.

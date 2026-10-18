# CLAUDE.md – dd-modern

Modernization mod for Divine Divinity (Steam/GOG 1.0062A) as an `.asi` plugin loaded by Ultimate ASI Loader. Full plan: [PLAN.md](PLAN.md) – **read "Current status" there first**. Work through the phases in order.

## Language

- **Talk to the human in German.**
- **Everything in the repo is English:** code comments, commit messages, README, `docs/`, `CLAUDE.md`, `PLAN.md`, `ddmod.ini` comments and log output.

## Paths (dev PC, Windows 11)

| What | Path |
|---|---|
| Repo | `C:\dev\dd-modern` |
| Original game (Steam, **German**, 1.0062A) | `C:\Program Files (x86)\Steam\steamapps\common\divine_divinity` – **read only, never modify** |
| Main working copy (1.0062A + dgVoodoo2 2.87.5 + ddmod) | `C:\dev\dd-test` – resolution 2560×1440 (keep), originals of changed configs as `*.orig` |
| Reference copy 1.0034A (on hold, do not change) | `C:\dev\dd-test-134` |
| Downloads (verified, outside repo) | `C:\dev\downloads` – `patch134_en\files` (unpacked patch 1.34A EN), `dgvoodoo2_87_5`, `dgvoodoo2_87_3`, `ultimate-asi-loader_9.7.4\dinput8.dll` |
| Screenshots / fetched logs (never commit) | `C:\dev\screenshots` (Deck files in `deck\`) |
| Ghidra 12.1.4 | `C:\tools\ghidra_12.1.4_PUBLIC` (JDK: Temurin 21) |
| Cheat Engine 7.7 | `C:\Program Files\Cheat Engine` |
| x32dbg | `x32dbg` (winget alias) |
| MSVC | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` (`dumpbin`: `VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\dumpbin.exe`) |
| 7-Zip | `C:\Program Files\7-Zip\7z.exe` |

Hardware: GTX 1050 Ti (DXGI adapter 1, primary monitor 2560×1440) + Intel UHD 630 (adapter 2, second monitor).

## Fixed rules

1. **No Larian files in the repo** (EXE, DLLs, graphics, texts, audio, fonts). Maintain `.gitignore`.
2. **No decompiled code / pseudocode in the repo** – not even reworded by an AI. Document findings in our own words in `docs/re-notes.md` (purpose, signature, address, structure layout). The implementation is always our own code.
3. **No binary patches of the EXE.** Everything goes through the plugin.
4. **Every feature can be switched off individually** via `ddmod.ini`. Invasive features (fast travel, balancing) are off by default.
5. **Generate derived content only locally on the user's machine** (upscaling, TTS). The mod ships tools, not data.
6. **No cloned voices of real voice actors** – only generic synthetic voices.
7. **Protect savegames:** back up saves before tests that write game state; test state-changing features with throwaway saves first.
8. **No hard-coded addresses** – use signature scans; fixed addresses only as a documented fallback for the EXE verified by SHA-256.
9. **Ask instead of guessing:** for in-game tests, write clear test instructions (what to do, what to observe) in `docs/testing.md` and wait for the result.
10. **Analyze only the game's own files** (EXE/DLLs). Do not disassemble third-party tools (dgVoodoo2, ASI loader).

## Working agreements with the human

- Ask before installing software (winget) and before any change on the Steam Deck; show the commands first.
- Downloads only from official sources, verify SHA-256 against the published digest; scripts must abort if no unique asset is found.
- The human keeps `PLAN.md` decisions; Claude maintains `PLAN.md` (status, results) and commits doc changes.
- **Context handover:** after every completed phase, after a larger sub-step, and after a longer debugging session, remind the human to clear the context (`/clear`). Before that: update `PLAN.md` ("Current status" and next steps), `CLAUDE.md` and `docs/re-notes.md` (open questions) so that a new session can continue seamlessly without the previous conversation, commit everything, and suggest a start prompt for the new session.

## Build

- The game is **32-bit** → always build x86.
- `cmake -S . -B build -G "Visual Studio 17 2022" -A Win32` then `cmake --build build --config Release` → `build/Release/ddmod.asi` (static CRT, `/W4` clean).
- One feature per branch. Current branch: see PLAN.md "Current status".

## Test workflow (Windows)

PowerShell module `tools/testrun/DdTest.psm1` (refuses the Steam folder):
- `Install-DdMod -GameDir C:\dev\dd-test [-ResetIni]` – copies ASI loader as `winmm.dll` (never replaces a foreign `winmm.dll`), `ddmod.asi`, `ddmod.ini` (kept if present).
- `Uninstall-DdMod -GameDir …` – removes `winmm.dll` (only if it is the loader), `ddmod.asi`, `ddmod.log`.
- `Set-DgVoodooProfile -GameDir … -Adapter 1 [-Watermark]`, `Set-DivConfig -GameDir … -Width … -Height …`.

`tools/testrun/Start-GameTest.ps1 -GameDir C:\dev\dd-test [-NoIntro] [-PressEscapeAt 4,8,…] -Seconds 15 -Count 2 -Interval 10`:
starts the game windowed (temporarily `FullScreenMode=false` in `dgVoodoo.conf`), saves screenshots to `C:\dev\screenshots`, kills the game, restores configs and the run markers `init.cfg`/`sinfo.000`, answers the "not correctly terminated" dialog with **No**, prints `ddmod.log`. View screenshots with the Read tool. Crash details: Windows event log, Application, ID 1000.

Pitfalls: the game treats a kill as a crash (run markers); **never answer "Yes"** to the reset dialog; after repeated crashes the game may reset `config.div` and delete `slashed-d3d6.cfg` by itself → restore from backup. The game rewrites `config.div` on exit. Resolution lives in `config.div` **and** `slashed-d3d6.cfg` (offsets 0x0C/0x10).

## Steam Deck access (SSH)

- `ssh deck@<deck-ip>` (key auth from the PC, `~/.ssh/id_ed25519`); always use `-o BatchMode=yes -o ConnectTimeout=15`. If unreachable: the Deck is asleep → ask the human to wake it.
- Run remote scripts from the **Bash tool** (Git Bash) with a heredoc: `ssh -o BatchMode=yes deck@<deck-ip> 'bash -s' <<'EOF' … EOF`. Do not pass quoted commands from Windows PowerShell 5.1 (it strips inner quotes) and do not pipe files with CRLF/BOM.
- `pgrep -f div.exe` matches its own command line – use `pgrep -x div.exe`.
- Game folder: `~/.local/share/Steam/steamapps/common/divine_divinity` (German, same `div.exe` hash as the PC; own `config.div` 1280×720, `rgb 565`). Savegames in `savegames/`.
- Backup of the original Deck state: `divine_divinity/_backup_ddmod_20261018-110251` (configs, run markers, all savegames). Disabled dgVoodoo files: `divine_divinity/_ddmod_dgvoodoo_disabled`. `config.div.broken` = config after the game's self-reset (kept for analysis).
- Proton log (with launch option `PROTON_LOG=1`): `~/steam-214170.log`, overwritten on every start. Screenshots (Steam+R1): `~/.local/share/Steam/userdata/*/760/remote/214170/screenshots/`.
- Launch options are set by the human in Steam (Game Mode); hand them over as a text file on `~/Desktop`. Current Deck state: original, no dgVoodoo, Proton 10.0 selected, launch option removed. For the mod: `WINEDLLOVERRIDES="winmm=n,b" %command%`.

## Workflow per feature

Investigate → document in `docs/re-notes.md` → implement minimally behind a feature flag → test instructions in `docs/testing.md` → expand → commit. Test every graphics/overlay change on PC **and** Deck.

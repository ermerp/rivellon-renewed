# CLAUDE.md – Rivellon Renewed

**Rivellon Renewed** – a modernization mod for Divine Divinity (Steam/GOG 1.0062A) as an `.asi` plugin loaded by Ultimate ASI Loader. Files in the game folder: `RivellonRenewed.asi/.ini/.log/.prev.log` and the folder `RivellonRenewed\` (`fonts\`, `logs\`); the file stem is set once in `CMakeLists.txt` (`MOD_FILE_STEM`). Internal identifiers keep the short name `ddmod` (namespace, CMake targets, `Install-DdMod`); local folder `C:\dev\dd-modern`, GitHub repo `ermerp/rivellon-renewed`.

This file holds only rules that apply in every session. Workflows and pitfalls: `docs/dev/workflow.md`, Steam Deck: `docs/dev/steam-deck.md`.

## Where things are

| What | File |
|---|---|
| Status, next step, v0.01 path | `PLAN.md` |
| Finished steps, release notes ("Unreleased") | `CHANGELOG.md` |
| Long-term phases | `docs/roadmap.md` |
| Decisions / rejected approaches (one line + why) | `docs/decisions.md` / `docs/rejected.md` |
| Incidents behind rules | `docs/lessons.md` |
| RE findings by topic (index first) | `docs/re/README.md`, open questions in `docs/re/open-questions.md` |
| Test instructions for the human / one-line results | `docs/testing.md` / `docs/test-results.md` |
| Commands, tools, pitfalls / Deck / input sequences | `docs/dev/workflow.md` / `docs/dev/steam-deck.md` / `docs/dev/test-sequences.md` |
| Old full texts (not maintained, search only) | `docs/archive/` |

## Session start (`/catchup`)

Read this file, `PLAN.md`, the first 10 lines of the table in `CHANGELOG.md`, `git log --oneline -10` and `git status`. Everything else only when the task needs it.

## Session wrap-up (`/wrapup`)

- `PLAN.md` "Current status": replace, don't add – where we are, next concrete step, what waits for the human, state of the working copies.
- `CHANGELOG.md`: one line per finished step (and the user-facing line under "Unreleased" for a new feature).
- `docs/decisions.md` / `docs/rejected.md`: one line each, with the reason. An incident that created a rule: one row in `docs/lessons.md`.
- Findings into the matching `docs/re/<topic>.md` (a new file gets a line in `docs/re/README.md`); keep `open-questions.md` current. Test results: one line in `docs/test-results.md`.
- New commands, tools and pitfalls go to `docs/dev/`, not here.
- **Size check:** a file over ~15 KB (except `docs/archive/`) → propose a split.
- Propose a commit message (English, with the attribution line) and **commit only after the human confirms**.

## Language

- **Talk to the human in German.**
- **Everything in the repo is English:** code comments, commit messages, docs, `RivellonRenewed.ini` comments, log output.

## Fixed rules

1. **No Larian files in the repo** (EXE, DLLs, graphics, texts, audio, fonts, screenshots). Maintain `.gitignore`.
2. **No decompiled code / pseudocode in the repo** – not even reworded by an AI. Findings in our own words in `docs/re/` (purpose, signature, address, layout); the implementation is always our own code.
3. **No binary patches of the EXE.** Everything goes through the plugin.
4. **Every feature can be switched off individually** in `RivellonRenewed.ini`. Invasive features (fast travel, balancing) are off by default.
5. **Generate derived content only locally on the user's machine** (upscaling, TTS). The mod ships tools, not data.
6. **No cloned voices of real voice actors** – only generic synthetic voices.
7. **Protect savegames:** back up saves before tests that write game state; test state-changing features with throwaway saves first.
8. **No hard-coded addresses** – signature scans; fixed addresses only as a documented fallback for the EXE verified by SHA-256.
9. **Ask instead of guessing:** for in-game tests, write clear instructions (what to do, what to observe) in `docs/testing.md` and wait for the result. What was built on an assumption is documented as an assumption.
10. **Analyze only the game's own files** (EXE/DLLs), not third-party tools (dgVoodoo2, ASI loader); disassembly output to the scratchpad only. If analyzing a third-party tool would help a lot, say so and why – the human decides.
11. **Screenshots only of the game window, never of the desktop.** No game window (exited, not started, another program's dialog) → no screenshot, report it. Never open desktop-sized images from our own scripts. **Exception – the human's captures:** when the human says they took screenshots, open images in `%USERPROFILE%\Videos\Captures` from that test period whose file name names the game window (`Divinity …`), skip anything else, never copy them into the repo. **Exception – Steam Deck:** any screen may be captured there.
12. **Stay true to the original look:** new visible elements follow the game's original style; faithfulness takes precedence over a modern look. For design decisions show the human variants side by side (images outside the repo) and let them choose.
13. **No system dependencies:** bundled resources (free-licensed fonts) and our own code instead of what an OS or runtime provides, so one build looks and behaves the same on Windows and Proton. Unavoidable system parts (DirectDraw, input) sit behind a thin hook layer, noted in `docs/re/`.

## Documentation rules

- **One place per fact:** every fact stands in exactly one file (table above); the others link to it.
- **Update instead of append:** change the existing statement, delete what is outdated. No history in the maintained docs – dates only in `CHANGELOG.md`, `docs/lessons.md`, `docs/test-results.md`; `docs/archive/` is never updated.
- **When shortening or moving text,** compare against the old version and report which numbers, addresses and dates are gone.
- `CLAUDE.md` stays under ~8 KB: rules only, no findings, no status.

## Paths (dev PC, Windows 11)

| What | Path |
|---|---|
| Original game (Steam, German, 1.0062A) | `C:\Program Files (x86)\Steam\steamapps\common\divine_divinity` – **read only, never modify** |
| Working copies (1.0062A + dgVoodoo2 2.87.5 + mod, 2560×1440, originals of changed configs as `*.orig`) | `C:\dev\dd-test` (DE), `C:\dev\dd-test-en` (EN), `C:\dev\dd-test-fr` (FR), `C:\dev\dd-test-ru` (RU, **own EXE 1.0061**) |
| **Play copy of the human** (1920×1080, coverage always on; feedback in `docs/playtest-log.md`) | `C:\dev\dd-play` – the human's **real saves**: no automated tests, never touch `savegames\`; mod updates only after asking, with a save backup first (`C:\dev\backups\dd-play-savegames-<YYYYMMDD-HHMMSS>`) |
| Reference copy 1.0034A (on hold, do not change) | `C:\dev\dd-test-134` |
| Test saves (one per situation, `T-*`) | `C:\dev\testsaves\de` – copy into a working copy's `savegames\`, never over the human's own saves |
| Downloads (verified, outside repo) | `C:\dev\downloads` |
| Screenshots / fetched logs (never commit) | `C:\dev\screenshots` (Deck: `deck\`) |
| Tools (Ghidra, Cheat Engine, x32dbg, MSVC, 7-Zip) | `docs/dev/workflow.md` |

## Working agreements

- Ask before installing software (winget) and before any change on the Steam Deck; show the commands first.
- **The Deck is usually off:** announce beforehand when a task needs it and wait until the human has switched it on – no connection attempts without that.
- **Real input (`SendInput`, `-Steps`, `-PressHotkeyAt`) only when the human has said the PC is free.**
- The repo is on GitHub: no private data in files or commits – `%USERPROFILE%` instead of the Windows user path, `<deck-ip>` instead of LAN addresses (real values in Claude's local memory); commits use the noreply address from `git config user.email`. Secret scan of the history before the repo goes public.
- Downloads only from official sources, verify SHA-256 against the published digest; scripts abort if no unique asset is found.
- The human keeps the decisions; Claude maintains `PLAN.md`, the docs and commits doc changes.
- **Git:** one branch per feature or fix, created before the first file is touched; merge into `main` with `--no-ff` after the test, then delete the branch.
- **Context handover:** after a finished phase, a larger sub-step or a long debugging session, suggest `/wrapup`, then `/clear`, then `/catchup` (optionally with the task, e.g. `/catchup v0.01 step 2`).

## Release

- **Docs hygiene check before every release:** (a) are the rules in this file and the documentation rules still current and useful? Name outdated or never-applied rules. (b) Are they kept? File sizes (≤ 15 KB except `docs/archive/`), every fact in one place only, no dead links, `PLAN.md` status current, `CHANGELOG.md` "Unreleased" complete as release notes, `docs/decisions.md`/`docs/rejected.md` without contradictions to the code. Result as a report with proposals to the human – change nothing silently.

## Workflow per feature

Investigate → document in `docs/re/` → implement minimally behind a feature flag → test instructions in `docs/testing.md` → expand and harden → commit. **Every phase is tested in `dd-test` (DE) and `dd-test-en` (EN)**; every graphics/overlay change on PC **and** Deck. Build and test commands: `docs/dev/workflow.md`.

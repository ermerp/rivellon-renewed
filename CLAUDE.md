# CLAUDE.md – dd-modern

Modernization mod for Divine Divinity (Steam/GOG 1.0062A) as a proxy DLL. Full plan: [PLAN.md](PLAN.md). Work through the phases in order.

## Language

- **Talk to the human in German.**
- **Everything in the repo is English:** code comments, commit messages, README, `docs/`, `CLAUDE.md`, `PLAN.md`, `ddmod.ini` comments and log output.

## Paths

- Original game (Steam, German language version): `C:\Program Files (x86)\Steam\steamapps\common\divine_divinity` – **read only, never modify.**
- Test working copy: `C:\dev\dd-test` – DLLs, dgVoodoo2, INI etc. are installed and tested here.
- Repo: `C:\dev\dd-modern`
- Ghidra 12.1.4: `C:\tools\ghidra_12.1.4_PUBLIC` (JDK: Temurin 21)
- x32dbg: `x32dbg` (winget alias)
- MSVC: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`

## Fixed rules

1. **No Larian files in the repo** (EXE, DLLs, graphics, texts, audio, fonts). Maintain `.gitignore`.
2. **No decompiled code / pseudocode in the repo** – not even reworded by an AI. Document findings in our own words in `docs/re-notes.md` (purpose, signature, address, structure layout). The implementation is always our own code.
3. **No binary patches of the EXE.** Everything goes through the proxy DLL.
4. **Every feature can be switched off individually** via `ddmod.ini`. Invasive features (fast travel, balancing) are off by default.
5. **Generate derived content only locally on the user's machine** (upscaling, TTS). The mod ships tools, not data.
6. **No cloned voices of real voice actors** – only generic synthetic voices.
7. **Protect savegames:** back up saves before tests that write game state; test state-changing features with throwaway saves first.
8. **No hard-coded addresses** – use signature scans; fixed addresses only as a documented fallback for the EXE verified by SHA-256.
9. **Ask instead of guessing:** for in-game tests, write clear test instructions (what to do, what to observe) in `docs/testing.md` and wait for the result.

## Technology

- The game is **32-bit** → always build x86 (MSVC `-A Win32` or an x86 toolchain).
- Build: CMake (+ Ninja), MSVC Build Tools 2022.
- Analyze only the game EXE/DLLs (content is sent to the model provider).

## Workflow per feature

Investigate → document in `docs/re-notes.md` → implement minimally behind a feature flag → test instructions in `docs/testing.md` → expand → commit. One feature per branch.

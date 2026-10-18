# Development environment

Windows 11 Home (build 26200). Paths: see CLAUDE.md.

| Tool | Version | Source |
|---|---|---|
| Git | 2.55.0 | winget |
| VS Build Tools 2022 (MSVC x86) | 17.14 | winget |
| CMake / Ninja | 4.4.4 / 1.13.2 | winget |
| Python | 3.13.15 | winget (no Pillow) |
| JDK (Temurin) | 21.0.12 | winget |
| Node.js | 24.20.0 | winget |
| x64dbg (incl. x32dbg) | 2026.05.27 | winget |
| 7-Zip | 26.04 | winget |
| Ghidra | 12.1.4 | official NSA GitHub release, SHA-256 verified |
| Cheat Engine | 7.7 | installed by the human (64-bit build attaches to the 32-bit game; `cheatengine-i386.exe` available) |
| MinHook | 1.3.4 | CMake FetchContent, pinned commit `c3fcafdc…` |

Quick static analysis without Ghidra: `dumpbin /disasm:nobytes [/range:a,b] div.exe` (output only to the scratchpad, never into the repo).

## Downloads (verified)

- **Ultimate ASI Loader 9.7.4:** `github.com/ThirteenAG/Ultimate-ASI-Loader`, `Ultimate-ASI-Loader-NoPDB.zip` (SHA-256 = GitHub digest); x86 `dinput8.dll` SHA-256 `EC2F4824ECA58DD40F425756A4A7CEC77B8E381F21D11D7C846EC4B339B617AB`, installed as `winmm.dll` (forwards all WINMM exports).
- **dgVoodoo2 2.87.5:** `github.com/dege-diosg/dgVoodoo2`, `dgVoodoo2_87_5.zip` SHA-256 `5ffde6927f7355ca3fdd5d785b581256a8e6539fa13e395a891ade6ba1040850` (= GitHub digest). 2.87.3 also downloaded for a later Deck test.

## dgVoodoo2 setup (working copies)

- Copy `MS\x86\DDraw.dll`, `MS\x86\D3DImm.dll`, `dgVoodooCpl.exe` and the default `dgVoodoo.conf` into the copy; apply settings with `Set-DgVoodooProfile -Adapter 1` (`tools/testrun/DdTest.psm1`): D3D11 FL 11.0, adapter 1, all general checkboxes off except color profile, no passthru, virtual 3D card, 128 MB, app-driven quality, only vSync forced, `AppControlledScreenMode = false` (lets the test script switch windowed/full screen).
- DXGI adapters on the dev PC: 1 = GTX 1050 Ti (primary monitor 2560×1440), 2 = Intel UHD 630, 3 = Basic Render Driver.
- Game settings with `Set-DivConfig`: Direct3D (`slashed.cfg` = 0), smoothness on, alpha bit off, resolution (also in `slashed-d3d6.cfg`).
- Works on Windows at 1920×1080 and 2560×1440 (no > 1080p crash; 4GB patch not needed so far). **Crashes under Proton on the Deck** (2.87.5) → the Deck runs without dgVoodoo2. Drawbacks: no Steam overlay; main menu shown at a different size than the game. 32-bit games should be Large Address Aware with dgVoodoo (author's note).

## Smart App Control

Disabled on the dev PC (2026-10-17; registry `VerifiedAndReputablePolicyState = 0`) because it blocked Cheat Engine and unsigned builds. For users (Phase 10): with SAC on, the unsigned ASI loader and `ddmod.asi` may be blocked → README known issue; consider code signing.

# RE notes

Findings in our own words. **No decompiled code, no pseudocode** (see CLAUDE.md, rule 2).

## 0. Setup

Development machine: Windows 11 Home (build 26200).

| Tool | Version | Location / source |
|---|---|---|
| Git | 2.55.0 | winget |
| VS Build Tools 2022 (MSVC x86) | 17.14 | `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` (winget) |
| CMake / Ninja | 4.4.4 / 1.13.2 | winget |
| Python | 3.13.15 | winget |
| JDK (Temurin) | 21.0.12 | winget |
| Node.js | 24.20.0 | winget |
| x64dbg (incl. x32dbg) | 2026.05.27 | winget |
| 7-Zip | 26.04 | winget |
| Ghidra | 12.1.4 | `C:\tools\ghidra_12.1.4_PUBLIC` (official NSA GitHub release, SHA-256 verified) |
| Cheat Engine | 7.7 | `C:\Program Files\Cheat Engine` (installed by the human; the 64-bit build can attach to the 32-bit game, `cheatengine-i386.exe` is also available) |
| dgVoodoo2 | 2.87.5 | in the working copies, see §5 |

**Smart App Control is disabled** on the development machine (by the human, 2026-10-17). Reason: it blocked Cheat Engine and would also block our own unsigned proxy DLL / `.asi`. Verified via registry: `HKLM\SYSTEM\CurrentControlSet\Control\CI\Policy\VerifiedAndReputablePolicyState = 0` (off).
Consequences:
- Unsigned binaries (our builds, game tools) can run without SAC prompts; Microsoft Defender remains active.
- **Relevant for users (Phase 10):** on systems with Smart App Control enabled, the unsigned mod DLL and ASI loader may be blocked. Document this in the README / known issues; consider code signing for releases.

## 1. Game version (Steam, German language version)

- Version according to the resources of `div.exe`: FileVersion `1, 0, 0, 62` (= 1.0062A)
- Installation folder: 363 files, approx. 2.4 GB; language according to `config.lcl`: `language 2` (German)

| File | Size (bytes) | SHA-256 |
|---|---|---|
| div.exe | 2 461 696 | `CD9F7A07C7A605C21052B3517346C7A63083063FB491276A3E4C9836C9B42A67` |
| configtool.exe | 280 576 | `053B19740948523FD5F785750BCD1771E8017A94A064F9AE203E5ED6AF339419` |
| DivDialogSystem.dll | 95 232 | `10C16C63B8881DD47160CD9DC876831EC9141C7A985C186AADF22499832C747C` |
| OsirisDLL.dll | 356 352 | `7D5F9E81F4E8C310C0FAF8FFA34E9872BFA2B07CA28E62C17E6E90412F6A0F95` |
| slash1.dll | 339 456 | `B2590BFE86956F142560558691BEBC489C4F3C0D761C3CEFCD8F75DEBD2EF3A5` |
| binkw32.dll | 357 939 | `65D87646770FA871A58D9D27CDFB5A1AD991DD64900F0EA35B0678457C902D41` |
| fmod.dll | 127 488 | `1DF3354B63EC6668A4831FCD02D12F36A6E4707435505F5C55DAD2F4AB62EEE9` |

### PE header `div.exe` (1.0062A)

- Linker 9.00 (VS 2008), timestamp 2013-04-19, runtime `MSVCR90.dll`
- Image base `0x00400000`, **relocations stripped, no ASLR** (DLL characteristics `0x8000` = terminal-server-aware only) → addresses within this build are the same on every start. Rule 8 still applies (signature scan, fixed addresses only as a fallback).
- **Large Address Aware not set** (characteristics `0x103`) → max. 2 GB of address space without the 4GB patch.

## 2. Imports and runtime architecture (1.0062A)

Static dependencies of `div.exe` (number of imported functions):

| DLL | # | Purpose / key functions |
|---|---|---|
| fmod.dll | 32 | Audio via **FMOD 3.x** (`FSOUND_*`: samples, streams, 3D sound, volume) |
| binkw32.dll | 9 | Videos via **Bink** (`BinkOpen`, `BinkDoFrame`, `BinkCopyToBuffer`, `BinkOpenDirectSound`) |
| DivDialogSystem.dll | 12 | Dialog engine as the C++ class `CDivDialogSystem` |
| OsirisDLL.dll | 14 | Osiris script VM as the C++ class `COsiris` (global instance exported) |
| USER32.dll | 28 | Windows/messages, `GetAsyncKeyState`, `GetKeyState`, `MapVirtualKeyA`, `GetCursorPos` |
| GDI32.dll | 8 | Fonts (`CreateFontA`, `GetGlyphOutlineA`) – probably text rendering for Unicode languages |
| IMM32.dll | 3 | IME text input |
| KERNEL32.dll | 54 | among others `LoadLibraryA`, `GetProcAddress`, `SetThreadAffinityMask` |
| VERSION.dll | 3 | `GetFileVersionInfo*` |
| WINMM.dll | 1 | only `timeGetTime` |
| MSVCR90.dll | 170 | C runtime |

Results for the questions from PLAN.md Phase 0, step 3:

- **Graphics:** `div.exe` imports **no** DirectDraw/Direct3D. The renderers are plugins loaded at runtime via `LoadLibraryA`/`GetProcAddress`. All four export the same interface (`DllSlashedInit`, `DllSlashedStartFrame`, `DllSlashedEndFrame`, `DllSlashedGetResolutions`, `DllSlashedGlowDraw*`, `DllSlashedShutdown`, …):

  | DLL | Renderer | Config | Graphics API |
  |---|---|---|---|
  | slash1.dll | Direct3D 6.x | `slashed-d3d6.cfg` | `ddraw.dll` (DirectDraw + Direct3D 6) |
  | slash2.dll | Glide 3.x | `slashed-glide.cfg` | `glide3x.dll` (not present) |
  | slash3.dll | DirectDraw | `slashed-directx.cfg` | `ddraw.dll` |
  | slash4.dll | Software | `slashed-software.cfg` | `ddraw.dll` (output only) |

  So the Direct3D renderer also runs through `ddraw.dll` (D3D6 is created via the DirectDraw object; `D3DImm.dll` is loaded internally by `ddraw.dll`). dgVoodoo2 (`ddraw.dll` + `D3DImm.dll`) therefore covers Direct3D, DirectDraw and Software.
  **Consequence for the mod:** `DllSlashedStartFrame`/`DllSlashedEndFrame` provide a renderer-independent frame tick – a good hook point for per-frame logic (e.g. by intercepting `GetProcAddress` for these names).
- **Input:** **No DirectInput.** Keyboard/mouse via window messages (`PeekMessageA`/`GetMessageA`) plus `GetAsyncKeyState`/`GetKeyState`/`GetCursorPos`. → Input hook via the window procedure (subclassing) and hooks on these USER32 functions.
- **Audio:** FMOD 3.x (`fmod.dll`, uses DirectSound/WINMM internally). No Miles. For ducking (Phase 7), `FSOUND_SetVolume`/the stream functions are suitable.
- **Video:** Bink (`binkw32.dll`), sound via `BinkOpenDirectSound`.
- **Dialogs:** `DivDialogSystem.dll` exports among others `StartDialog`, `GetNumQuestions`, `GetQuestion` (returns a wide string), `GetAnswerText`, `GetAnswerSoundName`, `SelectQuestion`, `GetQuestionNodeID`, `GetAnswerNodeID`. → For Phase 6 (custom dialog box), text and selection can be captured directly at these exports without analyzing the EXE.
- **Osiris:** `OsirisDLL.dll` exports `COsiris::Event`, `Load`, `Save`, `InitGame`, `GetStoryVersion`, `ApplyPatchFile` and others – the basis of OsirisRE.

Other DLLs in the folder: `configtool.exe` imports `DDRAW.dll` (resolution list); `NlsEng/NlsFr/NlsGer.dll` need `mfc90.dll`; `DrvMgt.dll`/`SECDRV.SYS` are copy protection leftovers and are not imported by `div.exe`.

## 3. Decisions

### Proxy DLL (Phase 0, step 4) – decided

`div.exe` statically imports only two system libraries that are suitable as a proxy: **`winmm.dll`** (1 function) and **`version.dll`** (3 functions). `dinput.dll` is **out of the question** (it is never loaded). `ddraw.dll`/`D3DImm.dll` are reserved for dgVoodoo2.
→ Confirms the favorite: **Ultimate ASI Loader as `winmm.dll`**, the mod as an `.asi`. `version.dll` as a fallback. The 1.0034A EXE imports both names as well. Note: `fmod.dll` and `binkw32.dll` also import `WINMM.dll` – they then get the same proxy instance, so the loader must forward all WINMM exports completely (which is the case for Ultimate ASI Loader).

dgVoodoo2 also ships an optional `WinMM.dll` ("OS uptime fixer", patches `timeGetTime`/`QueryPerformanceCounter`). DD does not need it; it would collide with the ASI loader's `winmm.dll` and must not be installed alongside it.

### Overlay (Phase 0, step 5) – decided: option (b)

Agreed with the human on 2026-10-17:
- **Primary backend:** dgVoodoo2 translates the game's DirectDraw/D3D6 output to Direct3D 11. The overlay hooks the D3D11/DXGI present of that output and draws with the ImGui DX11 backend.
- **Swappable backend:** the overlay talks to the graphics layer only through a small backend interface in `src/overlay/` (init, begin frame, render draw data, resize, shutdown), so backends can be exchanged without touching overlay or feature code.
- **Fallback backend:** drawing via the `DllSlashedEndFrame` hook, i.e. onto the renderer's own back buffer before it is presented; works without dgVoodoo (e.g. under Proton if dgVoodoo does not work there).
- **Frame logic** (input polling, feature updates) always runs on `DllSlashedStartFrame`/`DllSlashedEndFrame`, independent of the graphics backend.
- Options (a) (drawing on the DirectDraw surface as the *only* path) and (c) (separate transparent window: problems with full screen, focus and Proton) were not chosen; (a) survives as the fallback backend.

## 4. Patch 1.34A (English) – content analysis

Source: `C:\dev\downloads\DivPatch_English.exe` (10 883 072 bytes, SHA-256 `132BE8A2ED741FB055449FB4983D0CCD5016C0D967ACCE6B0A21088D979B3D01`, not signed). Self-extracting RAR inside a PE resource; unpacked with 7-Zip (`7z x -trar`) to `C:\dev\downloads\patch134_en\files`, **not executed**. `patch.log`: "Divine Divinity Patch V1.0 - 13-03-03 - English."

Contents (80 files) and comparison with the German Steam installation:

| File(s) | Steam (DE, 1.0062A) | Language-dependent? |
|---|---|---|
| `div.exe` (1.0.0.34, 2003-05-02) | different | no – the EXE is multilingual (contains German CD messages, language tables DE/EN/FR) |
| `Osiris.dll` | missing (Steam's is called `OsirisDLL.dll`) | no |
| `DivDialogSystem.dll` (1.5.0.1) | different | no – but a **different interface** (strings as `unsigned short*` instead of `wchar_t*` in the mangled name) → only matches the 1.0034A EXE |
| `slash1–4.dll` | different (older builds) | no |
| `DoSomething.exe` ("DivHeroPatch MFC Application") | missing | helper program of the installer, not needed |
| `Dat\global.cmp`, `Dat\treasure.cmp` | different | no (word statistics identical to the Steam file) |
| other `Dat\*` | identical | – |
| `Global\region.000–007` | different (same size) | no |
| `Global\eggs.000`, `location.*` | identical | – |
| `Main\startup.v2\*` (49 files) | missing; compared to `main\startup`, 20 files are changed (among others `story.000/001`, `items.000`, `data.000`, `world.x0`, `shroud.x*`, `inv.*`), 2 are new (`mapv.1`, `mapv.2`), 27 identical | no – no language texts found; `static\books.000` and `quest_log.000` are identical to Steam DE |
| `Localizations\English\dlg.v2\dialogids.dat` | identical to DE | no |
| `Localizations\English\dlg.v2\dialogs.div` | **identical to Steam `german\dlg.v2\Male\dialogs.div`** | no (dialog structure) |
| `Localizations\English\dlg.v2\dialogtxt.dat` | different | **yes – English dialog text** |

Important observations:
- The German Steam version **already** contains `localizations\german\dlg.v2\Male|Female` with the 1.33 dialog structure. Only `main\startup.v2` is missing.
- The 1.0062A EXE knows neither `dlg.v%d` nor `startup.v%d` (the strings are missing) – so it does not use `dlg.v2` and `startup.v2`. The 1.0034A EXE looks for exactly `localizations\%s\dlg.v%d` and `main\startup.v%d`.
- The only language-dependent file in the patch: `dialogtxt.dat` (English). It is not needed for a German installation, because the German counterpart already exists.
- The 1.0034A EXE contains CD prompt texts ("Bitte legen Sie die CD namens %s …", `cdrom path`) and imports `unrar.dll` (present in Steam), `MSVCRT.dll`, `Osiris.dll`.

### PE header `div.exe` (1.0034A)

SHA-256 `E5664343DA564BF015CD0136E50FD1A79FC1AF1B84EAFE4D9A8760C17E427639`, linker 6.00 (VC6), image base `0x00400000`, no ASLR, not Large Address Aware.

### Working copy `C:\dev\dd-test-134` (variant A)

Copy of `C:\dev\dd-test` (German, 1.0062A) with these files taken over from the English patch (hashes verified after copying):
`div.exe`, `Osiris.dll`, `DivDialogSystem.dll`, `Dat\global.cmp`, `Dat\treasure.cmp`, `Global\region.000`–`region.007`, `main\startup.v2\` (49 files).
Not taken: `Localizations\English\`, `slash1–4.dll` (Steam versions kept), `DoSomething.exe`, `patch.log`. Left in place from Steam: `OsirisDLL.dll` (unused by 1.0034A), `main\startup\`, `localizations\german\` incl. `dlg.v2`.

## 5. dgVoodoo2

- Version 2.87.5, official release `github.com/dege-diosg/dgVoodoo2`, `dgVoodoo2_87_5.zip` SHA-256 `5ffde6927f7355ca3fdd5d785b581256a8e6539fa13e395a891ade6ba1040850` (matches the GitHub digest).
- Installed in `C:\dev\dd-test` and `C:\dev\dd-test-134`: `DDraw.dll` and `D3DImm.dll` from `MS\x86`, `dgVoodooCpl.exe`, default `dgVoodoo.conf` (so the control panel stores settings per folder).
- Release note from the author: 32-bit games should be Large Address Aware with a modern GPU driver, because dgVoodoo adds address-space overhead → matches the 4GB patch recommendation in PLAN.md.

### Applied configuration (2026-10-18)

Applied with `tools/testrun/DdTest.psm1` (`Set-DgVoodooProfile -Adapter 1 -Watermark`, `Set-DivConfig`) to both working copies; originals kept as `*.orig`.
- DXGI adapter order on the dev machine: 1 = NVIDIA GeForce GTX 1050 Ti (primary monitor, 2560×1440), 2 = Intel UHD 630 (second monitor), 3 = Microsoft Basic Render Driver → `Adapters = 1`.
- `AppControlledScreenMode = false` (corresponds to the guide's "all checkboxes off"); this also lets `FullScreenMode` decide windowed vs. full screen, which the test script uses.

## 6. Game configuration files

- `config.div` (plain text, `key value` lines, `//` comments): `width`, `height`, `smooth`, `alpha bit` ("Use alpha" in `configtool.exe`), `startup logo` (intro movies), `no fog`, `cdrom path` (1.0034A) and more. Both EXE versions read the same keys. **The game rewrites `config.div` on exit** (values are kept).
- `slashed.cfg` (4 bytes): selected renderer = first DWORD. The per-renderer files `slashed-<name>.cfg` start with their own ID: `0` = Direct3D 6 (`slashed-d3d6.cfg`), `1` = Glide, `2` = DirectDraw (`slashed-directx.cfg`), `3` = Software. They also contain a 800×600 resolution pair at offsets 0x0C/0x10 (meaning not yet verified; `config.div` decides the actual resolution).
- **Run marker:** the game creates `init.cfg` (4 bytes) and `sinfo.000` (8 bytes) in the game folder; neither exists in a fresh Steam installation. If the game was not shut down cleanly (crash, kill), the next start shows "Divinity was not correctly terminated … Do you want to reset the configuration?" – "Yes" would reset the configuration. The files stay after a clean exit; observed contents: after a killed run `init.cfg = 00 00 00 00`, `sinfo.000 = 01 00 00 00 01 00 00 00`; after a clean exit via the menu `init.cfg = 02 00 00 00`, `sinfo.000 = 01 00 00 00 02 00 00 00`. Exact meaning not yet known.
- Other files written at runtime: `persist.dat`, `keylist.txt`, `mapids.000`, `slashed-d3d6.cfg`.

## 7. Automated start tests (2026-10-18, `tools/testrun/Start-GameTest.ps1`, windowed, dgVoodoo watermark on)

| Copy | Setting | Result |
|---|---|---|
| `dd-test` (1.0062A) | 1920×1080, intro on | Runs; intro videos play (640×480 centered); no crash |
| `dd-test` (1.0062A) | 1920×1080, Esc at 4/8/12/16/20 s | Esc skips the videos → loading screen with tip → German main menu after ~25 s; version "V1.0062A" bottom right; menu centered at 1024×768 |
| `dd-test-134` (1.0034A) | 1920×1080, intro off | Runs; German main menu, version "V1.0034A"; menu drawn top-left (not centered) |
| `dd-test-134` (1.0034A) | 1920×1080, intro on | **Crash after 3–5 s**, access violation `0xC0000005` in `slash1.dll` (Steam build: offset `0x1d76d`; patch build: offset `0x1b0bd`) – reproducible |
| `dd-test-134` (1.0034A) | 1024×768, intro on | No crash, but the CD prompt "Bitte legen Sie die CD namens SPIEL-CD in das CD_Rom Laufwerk" appears instead of the videos |
| `dd-test-134` (1.0034A) | 1920×1080, intro on, `cdrom path` = working copy | Still crashes |

Interpretation (to be verified): 1.0034A does not find the movies (it expects them on the game CD) and shows the CD prompt instead; drawing that prompt / the video path crashes the renderer at resolutions above 1024×768 – independent of the `slash1.dll` build. In-game cutscenes are likely affected the same way. A fix would be a hook that redirects the CD lookup to the local `static\` folder (PLAN.md Phase 9). The 1.0034A crash at > 1080p reported by the community was not reproduced at 1920×1080 with intro off.

## 8. Version situation

- Steam/GOG 1.0062A is based on the 1.0029A code base (source code of 1.0034A lost); it has widescreen and Windows 7 adjustments, but not all fixes from 1.0034A. Savegames are not compatible between the versions.
- According to Larian (Raze, Steam forum), 1.0034A officially does not support resolutions above 1024×768; community reports say it works anyway with dgVoodoo → needs to be tested.
- According to RPG Codex (2003), 1.34 was also released for the German and French versions (German via CDV). An official source for the German patch has not been confirmed yet.

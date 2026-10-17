# RE notes

Findings in our own words. **No decompiled code, no pseudocode** (see CLAUDE.md, rule 2).

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

### Proxy DLL (Phase 0, step 4) – proposal

`div.exe` statically imports only two system libraries that are suitable as a proxy: **`winmm.dll`** (1 function) and **`version.dll`** (3 functions). `dinput.dll` is **out of the question** (it is never loaded). `ddraw.dll`/`D3DImm.dll` are reserved for dgVoodoo2.
→ Confirms the favorite: **Ultimate ASI Loader as `winmm.dll`**, the mod as an `.asi`. `version.dll` as a fallback. The 1.0034A EXE imports both names as well. Note: `fmod.dll` and `binkw32.dll` also import `WINMM.dll` – they then get the same proxy instance, so the loader must forward all WINMM exports completely (which is the case for Ultimate ASI Loader).

### Overlay (Phase 0, step 5) – open, to be agreed with the human

_See the evaluation in the conversation; the decision will be recorded here._

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
| `Main\startup.v2\*` (60 files) | missing; compared to `main\startup`, 21 files are changed (among others `story.000/001`, `items.000`, `data.000`, `world.x0`, `shroud.x*`, `inv.*`), the rest identical | no – no language texts found; `static\books.000` and `quest_log.000` are identical to Steam DE |
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

## 5. Version situation

- Steam/GOG 1.0062A is based on the 1.0029A code base (source code of 1.0034A lost); it has widescreen and Windows 7 adjustments, but not all fixes from 1.0034A. Savegames are not compatible between the versions.
- According to Larian (Raze, Steam forum), 1.0034A officially does not support resolutions above 1024×768; community reports say it works anyway with dgVoodoo → needs to be tested.
- According to RPG Codex (2003), 1.34 was also released for the German and French versions (German via CDV). An official source for the German patch has not been confirmed yet.

# Runtime architecture (1.0062A)

## Static imports of `div.exe`

| DLL | # | Purpose / key functions |
|---|---|---|
| fmod.dll | 32 | Audio via **FMOD 3.x** (`FSOUND_*`: samples, streams, 3D sound, volume) |
| binkw32.dll | 9 | Videos via **Bink** (`BinkOpen`, `BinkDoFrame`, `BinkCopyToBuffer`, `BinkOpenDirectSound`) |
| DivDialogSystem.dll | 12 | Dialog engine, C++ class `CDivDialogSystem` |
| OsirisDLL.dll | 14 | Osiris script VM, C++ class `COsiris` (global instance exported) |
| USER32.dll | 28 | Windows/messages, `GetAsyncKeyState`, `GetKeyState`, `MapVirtualKeyA`, `GetCursorPos`, `ShowCursor` |
| GDI32.dll | 8 | `CreateFontA`, `GetGlyphOutlineA` (probably Unicode-language text) |
| IMM32.dll | 3 | IME input |
| KERNEL32.dll | 54 | among others `LoadLibraryA`, `GetProcAddress`, `SetThreadAffinityMask` |
| VERSION.dll | 3 | `GetFileVersionInfo*` |
| WINMM.dll | 1 | only `timeGetTime` |
| MSVCR90.dll | 170 | C runtime |

## Graphics: renderer plugins

`div.exe` imports no graphics API. It loads one renderer plugin at runtime (`LoadLibraryA`/`GetProcAddress`), selected by `slashed.cfg` (`config-files.md`). All four export the same interface (`DllSlashedInit`, `DllSlashedStartFrame`, `DllSlashedEndFrame`, `DllSlashedGetResolutions`, `DllSlashedGlowDraw*`, `DllSlashedShutdown`, …):

| DLL | Renderer | Config | API |
|---|---|---|---|
| slash1.dll | Direct3D 6 | `slashed-d3d6.cfg` | `ddraw.dll` (D3D6 via the DirectDraw object; `D3DImm.dll` loaded by ddraw) |
| slash2.dll | Glide 3.x | `slashed-glide.cfg` | `glide3x.dll` (not present) |
| slash3.dll | DirectDraw | `slashed-directx.cfg` | `ddraw.dll` |
| slash4.dll | Software | `slashed-software.cfg` | `ddraw.dll` (output only) |

dgVoodoo2 (`ddraw.dll` + `D3DImm.dll`) therefore covers Direct3D, DirectDraw and Software. `DllSlashedStartFrame`/`EndFrame` give a renderer-independent frame tick. Direct3D is the only renderer with animated water reflections.

## Input, audio, video, dialogs, scripts

- **Input:** no DirectInput. Window messages (`PeekMessageA`/`GetMessageA`) plus `GetAsyncKeyState`/`GetKeyState`/`GetCursorPos` → ddmod subclasses the window procedure (`input.md`); the USER32 functions can be hooked if needed.
- **Audio:** FMOD 3.x (uses DirectSound/WINMM). Ducking (Phase 7): `FSOUND_SetVolume`, stream functions.
- **Video:** Bink, sound via `BinkOpenDirectSound`.
- **Dialogs:** `DivDialogSystem.dll` exports `StartDialog`, `GetNumQuestions`, `GetQuestion` (wide string), `GetAnswerText`, `GetAnswerSoundName`, `SelectQuestion`, `GetQuestionNodeID`, `GetAnswerNodeID` → Phase 6 can work at these exports.
- **Osiris:** `OsirisDLL.dll` exports `COsiris::Event`, `Load`, `Save`, `InitGame`, `GetStoryVersion`, `ApplyPatchFile`, … (basis of OsirisRE).
- Other files: `configtool.exe` imports `DDRAW.dll` (resolution list); `NlsEng/NlsFr/NlsGer.dll` need `mfc90.dll`; `DrvMgt.dll`/`SECDRV.SYS` are copy-protection leftovers, not imported.

## Decisions

- **Proxy:** Ultimate ASI Loader as `winmm.dll`, ddmod as `.asi`. `div.exe` imports only `winmm.dll` and `version.dll` as suitable system DLLs (`version.dll` = fallback; `dinput.dll` is never loaded; `ddraw.dll`/`D3DImm.dll` belong to dgVoodoo2). `fmod.dll`/`binkw32.dll` import WINMM too, so the loader must forward all WINMM exports (it does). Never install dgVoodoo2's optional `WinMM.dll` alongside it.
- **Overlay (2026-10-18):** base backend draws through the game's own DirectDraw/D3D6 at `DllSlashedEndFrame` – works with Windows ddraw, dgVoodoo2 and WineD3D (Deck). D3D11 via dgVoodoo2 only as an optional backend for Phase 8 shader effects. Backend swappable behind an interface in `src/overlay/`; per-frame logic runs on `DllSlashedStartFrame`/`EndFrame`. ImGui has no D3D6 backend → own renderer for ImGui draw lists.

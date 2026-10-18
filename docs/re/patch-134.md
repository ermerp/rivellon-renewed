# Patch 1.34A / EXE 1.0034A (on hold)

**Decision (2026-10-18):** 1.0062A is the main target. 1.0034A is on hold; `C:\dev\dd-test-134` stays as an untouched reference copy.

## Version history

1.0029A (release) → 1.0031A/1.0032A (hotfixes) → 1.0033A (patch 1.33, Apr. 2003) → 1.0034A (May 2003, fixes the black-map bug of 1.33, removes SafeDisc). Digital versions (1.0062A) use the 1.0029A game data. 1.34 also appeared in German (CDV, per RPG Codex) – no official source found.

## Patch contents

`C:\dev\downloads\DivPatch_English.exe` (SHA-256 `132BE8A2ED741FB055449FB4983D0CCD5016C0D967ACCE6B0A21088D979B3D01`, self-extracting RAR, unpacked with `7z x -trar` to `C:\dev\downloads\patch134_en\files`, never executed). 80 files:

- `div.exe` 1.0.0.34 (SHA-256 `E5664343DA564BF015CD0136E50FD1A79FC1AF1B84EAFE4D9A8760C17E427639`, VC6, base `0x400000`, no ASLR, not LAA) – multilingual; imports **`Osiris.dll`** (not `OsirisDLL.dll`), `unrar.dll`, `MSVCRT.dll`; contains CD prompts and `cdrom path`.
- `DivDialogSystem.dll` 1.5.0.1 – different interface (`unsigned short*` strings), only for the 1.0034A EXE. Older `slash1–4.dll`. `DoSomething.exe` = installer helper.
- Changed data: `Dat\global.cmp`, `Dat\treasure.cmp`, `Global\region.000–007`, `Main\startup.v2\` (49 files; vs. `main\startup`: 20 changed incl. `story.000/001`, `items.000`, `data.000`, `world.x0`, `shroud.x*`, `inv.*`; 2 new `mapv.1/2`).
- `Localizations\English\dlg.v2\`: `dialogs.div` and `dialogids.dat` identical to the German Steam `dlg.v2\Male`; only `dialogtxt.dat` is language-specific.
- The 1.0062A EXE has no `dlg.v%d`/`startup.v%d` strings → it ignores `dlg.v2` and `startup.v2`; the 1.0034A EXE uses them.

## Working copy `C:\dev\dd-test-134`

Copy of `dd-test` plus from the patch: `div.exe`, `Osiris.dll`, `DivDialogSystem.dll`, `Dat\global.cmp`, `Dat\treasure.cmp`, `Global\region.000–007`, `main\startup.v2\`. Steam `slash1–4.dll` kept.

## Test results (automated, windowed, dgVoodoo2)

- Intro off, 1920×1080: German main menu "V1.0034A", drawn top-left (not centered).
- Intro on, 1024×768: CD prompt "Bitte legen Sie die CD namens SPIEL-CD …" instead of the videos.
- Intro on, 1920×1080: crash after 3–5 s, access violation in `slash1.dll` (Steam build offset `0x1d76d`, patch build `0x1b0bd`); `cdrom path` does not help. Likely the CD-prompt/video path; a fix would redirect the CD lookup to `static\`.

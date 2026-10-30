# Developer workflow

Build, install, test scripts and tools on the Windows dev PC. Rules: `CLAUDE.md`; Steam Deck: [steam-deck.md](steam-deck.md); input sequences: [test-sequences.md](test-sequences.md).

## Build

- The game is **32-bit** → always build x86.
- `cmake -S . -B build -G "Visual Studio 17 2022" -A Win32`, then `cmake --build build --config Release` → `build/Release/RivellonRenewed.asi` (static CRT, `/W4` clean).
- Unit tests (no game needed): `ctest -C Release --test-dir build --output-on-failure` – read the whole output, never filter it.
- A new unit test must have failed once: build the defect it guards against in a separate `git worktree` (never by checkout in the working tree) and show the failure.

## Release zip (`tools/release/New-Release.ps1`)

- `.\tools\release\New-Release.ps1 [-Ref <commit>] [-Suffix rc1]` → `build\dist\RivellonRenewed-<version>[-<suffix>].zip` and `<zip>.sha256` (`MOD_VERSION` from `CMakeLists.txt`). Builds only committed files (`git archive` of `-Ref`, default `HEAD`; it warns about uncommitted changes), x86 Release, unit tests must pass; a few minutes (MinHook and stb are fetched fresh). **Reproducible:** the same commit gives a byte-identical zip (`/Brepro` in `CMakeLists.txt`, zip timestamps = commit time) – a different SHA-256 for the same commit means a changed toolchain or input.
- Fetches Ultimate ASI Loader from its GitHub release and its license from the tag's commit; pinned SHA-256 at the top of the script; the loader DLL must match `$AsiLoaderSha256` in `setup/setup.ps1`, so the uninstaller recognizes it. New version: `/release`, dependency check.
- Layout (paths in the game folder): `winmm.dll` (the loader), `RivellonRenewed.asi`, `RivellonRenewed.ini`, `RivellonRenewed-Setup.cmd`, `RivellonRenewed\setup.ps1`, `RivellonRenewed\licenses\` (`RivellonRenewed.txt` = our `LICENSE`, `Ultimate-ASI-Loader.txt`, `MinHook.txt` from the pinned checkout; stb_truetype under its public-domain option needs none), `RivellonRenewed\fonts\` (all of `assets\fonts`). The script reads the zip back and compares every entry with its source.
- Smoke test of a zip: `robocopy C:\dev\dd-steam-clean C:\dev\dd-test-first /MIR`, `Expand-Archive <zip> C:\dev\dd-test-first`, `RivellonRenewed\setup.ps1 -Install -NoPause`, `Start-GameTest.ps1 -NoIntro`, `setup.ps1 -Uninstall -NoPause`, then `robocopy … /L /MIR` – only files the game writes itself may differ.

## Install into a working copy (`tools/testrun/DdTest.psm1`)

The module refuses the Steam folder.
- `Install-DdMod -GameDir <copy> [-ResetIni]` – copies the ASI loader as `winmm.dll` (never replaces a foreign `winmm.dll`), `RivellonRenewed.asi`, `RivellonRenewed\fonts\`, `RivellonRenewed.ini` – rebuilt from the repo default every time (new keys and comments) with the values of the existing INI taken over (`-ResetIni`: pure default). Migrates a pre-rename install: removes `ddmod.asi`/`ddmod_fonts`, takes the values of `ddmod.ini` over (old file kept as `ddmod.ini.migrated`).
- `Install-DdMod` also copies the user setup (`RivellonRenewed-Setup.cmd`, `RivellonRenewed\setup.ps1`; `docs/re/environment.md`); it refuses `C:\dev\dd-steam-clean`.
- **Never-started copy for first-start tests** (`C:\dev\dd-test-first`): `robocopy C:\dev\dd-steam-clean C:\dev\dd-test-first /MIR` (fast after the first time, removes everything the game wrote), then dgVoodoo2's `MS\x86\DDraw.dll`, `D3DImm.dll`, `dgVoodooCpl.exe`, `dgVoodoo.conf` from `C:\dev\downloads\dgvoodoo2_87_5` (without the mod set `[GeneralExt] FullscreenAttributes = fake` yourself), optionally `Install-DdMod -ResetIni`. The state of the 2026-10-25 black-screen sighting is kept in `C:\dev\dd-test-fresh-oct25`.
- `Uninstall-DdMod -GameDir <copy>` – removes `winmm.dll` (only if it is the loader), `RivellonRenewed.asi`, `RivellonRenewed\fonts\` and **all logs** (`RivellonRenewed.log`, `RivellonRenewed.prev.log`, `RivellonRenewed\logs\`; also leftover `ddmod*` files) – copy the logs to `C:\dev\screenshots` first if they are still needed.
- `Set-ModIniValue -Path <copy>\RivellonRenewed.ini -Section … -Key … -Value …` – changes one INI value.
- `Set-DgVoodooProfile -GameDir … -Adapter 1 [-Watermark]`, `Set-DivConfig -GameDir … -Width … -Height …`.

**Log level:** `LogLevel=debug` in `RivellonRenewed.ini` logs every key-down and character message, focus change, sent window message, game font creation, `.fnt` file open and the characters each GDI font rasterizes. While a test round runs, all working copies stay on `debug`; when it ends, back to `info`.

## Automated start test (`tools/testrun/Start-GameTest.ps1`)

Starts the game from a working copy, takes screenshots of the game window to `C:\dev\screenshots` (never committed), closes the game and prints `RivellonRenewed.log`.

```powershell
# menu check without intro, two screenshots at 15 s and 25 s
.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test -NoIntro -Seconds 15 -Count 2 -Interval 10
# skip the intro with Esc
.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test -PressEscapeAt 4,8,12,16,20 -Seconds 6 -Count 4 -Interval 7
# both working copies
foreach ($dir in 'C:\dev\dd-test', 'C:\dev\dd-test-en') {
    Install-DdMod -GameDir $dir
    .\tools\testrun\Start-GameTest.ps1 -GameDir $dir -NoIntro -Seconds 15
}
```

- Windowed by default (temporarily sets `FullScreenMode = false` in `dgVoodoo.conf`); `-FullScreen` disables that.
- `-NoIntro` temporarily sets `startup logo 0`; `-Intro` temporarily sets `startup logo 1` in `config.div` and, where present, in `config.lcl` (overrides `config.div`; the English copy ships `startup logo 0` there) – use it so DE and EN run under the same conditions. `-PressEscapeAt` posts Esc at the given seconds.
- `-PostKeyAt` posts `-PostKeyVk` (default F3, no modifiers) to the game window – no real input, safe while someone else uses the PC; enough for a modifier-less hotkey.
- `-PressHotkeyAt` types the `-HotkeyVk` chord (default Ctrl+Shift+F3) as real input via `SendInput` – only if the game window could be brought to the foreground, otherwise it reports "NOT sent".
- `-Steps` runs an input script instead of the timed screenshots (`wait`, `mouse`, `click`/`rclick`/`dclick`, `press` (left button held), `key`, `hold`, `down`/`up` (only the key-down or key-up), `chord` (e.g. `chord Ctrl+Shift+F2`), `type`, `shot`, `state` (window style, size, focus), `alttab`, `refocus` (back like a taskbar click); syntax at the top of the script). Held keys repeat their key-down like a real keyboard. The script stops at the first step that cannot reach the game window.
- `-PressHotkeyAt` and `-Steps` are real input – **only when the human has said the PC is free**; nobody touches mouse or keyboard during the run.
- Restores `dgVoodoo.conf`, `config.div` (with `-NoIntro`) and the run markers `init.cfg`/`sinfo.000` afterwards, so the "not correctly terminated" dialog does not appear next time; if it appears anyway, the script answers **No**.
- Reports an early exit (crash) with its exit code; crash details: Windows event log, Application, event ID 1000.
- Captures **only the game's own window**; no game window, no shot (rule 11).

**Judging screenshots:** the Read tool shows 2560×1440 images downscaled, which fakes glyph defects in small text – judge text details only from full-resolution crops (System.Drawing in PowerShell, output to the scratchpad).

## Font comparison sheets (`ddmod_fontpreview`, rule 12)

`build/Release/ddmod_fontpreview.exe --out C:\dev\screenshots\fonts\<name>.png [--bg R,G,B]` plus entries (usage at the top of `tools/fontpreview/main.cpp`):
- `--fnt`, `--gdi`, `--ttf`, `--pixel`, `--smooth` – sample lines per source; `--overview <dir> <scale>`; `--charsheet <fnt> <scale> <threshold%> <label>`.
- `--symbols <dir> <scale>` – every non-letter slot per `.fnt` (finds symbol glyphs).
- `--scene <spec.txt>` – real game lines on a screenshot crop per candidate, drawn like the mod; spec format at the top of `tools/fontpreview/scene.cpp`; specs hold game text → scratchpad only.
- `--arrows` – option arrow variants on a screenshot crop.
- `--labels <text.cmp> …` – width of every menu label in the original and the menu face; prints game text → scratchpad only.

Renders with the overlay's text code; game `.fnt` files are read from a working copy (copies only in the scratchpad).

## README banner (`tools/banner/New-Banner.ps1`)

Renders the banner from the bundled Uncial Antiqua; Windows PowerShell 5.1, deterministic (same bytes on every run). README: `-Out docs\images\banner.png -Height 400 -Transparent`; social preview (1280×640 on dark stone, upload by hand under Settings → Social preview): `-Out docs\images\social-preview.png`.

## Function coverage (`ddmod_coverage`, `docs/re/engine-map.md`)

`build/Release/ddmod_coverage.asi` plus `ddmod_coverage.in` (line `timestamp <PE TimeDateStamp hex>`, optional line `defer-window`, then one function address per line, made from the Ghidra export in the scratchpad) into a working copy. Without `defer-window` it measures from the start, but `RivellonRenewed.asi` must be renamed away (its scans read the patched bytes); with `defer-window` it starts at the game window and runs beside the mod. Result per start `ddmod_coverage-<start time>.out` (address, ms of first hit) – copy them out, then remove the tool's files.

## Analysis tools and downloads

- **Hardware:** GTX 1050 Ti (DXGI adapter 1) + Intel UHD 630 (adapter 2), two 2560×1440 monitors.
- **Cheat Engine 7.7** `C:\Program Files\Cheat Engine`; **x32dbg** (winget alias `x32dbg`); **7-Zip** `C:\Program Files\7-Zip\7z.exe`; **MSVC** `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools` (`dumpbin`: `VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\dumpbin.exe`).
- **Ghidra 12.1.4** (`C:\tools\ghidra_12.1.4_PUBLIC`, JDK Temurin 21), headless: `support\analyzeHeadless.bat <project dir> <name> -import <file>` (later `-process <file> -noanalysis -readOnly`) `-scriptPath <dir> -postScript <Script>.java …` – project, scripts and output only in the scratchpad.
- **`C:\dev\downloads`** (verified, outside the repo): `patch134_en\files`, `dgvoodoo2_87_5`, `dgvoodoo2_87_3`, `ultimate-asi-loader_9.7.4\dinput8.dll`; versions and SHA-256 in `docs/re/environment.md`.
- **Play copy `dd-play`:** `LogLevel=info`, `ddmod_coverage.asi` with `defer-window` always on – collect its `ddmod_coverage-*.out` from time to time.

## Pitfalls

- Windows paths in Python string literals (e.g. in a heredoc script editing docs): `\b`, `\n`, `\t` become control characters (`docs\images\banner.png` lost its `\b`) – use raw strings or `/`, and grep the result for control characters.

- The game treats a kill as a crash (run markers); **never answer "Yes"** to the reset dialog – it resets the configuration.
- After repeated crashes the game may reset `config.div` and delete `slashed-d3d6.cfg` → restore from backup. Back up any config file before changing it in a test.
- The game rewrites `config.div` on exit. The resolution lives in `config.div` **and** `slashed-d3d6.cfg` (offsets 0x0C/0x10).
- `config.lcl` needs `flat files` and `quickstart` – without them the game does not start.
- Before a run check that no `div.exe` is left (`Get-Process div`): a second instance exits at once. A `div.exe` hung on exit ignores `Stop-Process`/`taskkill /F` – ask the human to end it in Task Manager, then check `config.div`, `dgVoodoo.conf` and the run markers of that copy.
- An aborted `Start-GameTest.ps1` leaves `dgVoodoo.conf` windowed and `startup logo` in `config.div` – restore both.
- Windowed runs at 2560×1440: the window (2562×1474) is taller than the screen and the taskbar covers its bottom – no `click` in the bottom part; use the game's keys (`keylist.txt`).
- `-FullScreen` runs: in the borderless full screen `PrintWindow` captures the game window normally; with `exclusive` it returns black and the script skips the shot (also a real all-black picture).
- In heredocs of the Bash tool, backslash escapes in inline Python and Windows paths in `sed` replacements get mangled – write scripts with the Write tool and change text with the Edit tool.
- A new git repo (e.g. `local\`) takes the global e-mail – set `git config user.email` to the noreply address in it before the first commit. `git merge -F -` reads no message from a heredoc here; use `-m`.
- `build\release` and `build\Release` are the same folder (case-insensitive) – release output goes to `build\dist`.
- Fandom wikis: Cloudflare answers `curl` on pages and files with a bot check (never work around it); the MediaWiki API (`<wiki>/api.php?action=query…`) returns page text and CSS; files the human downloads in the browser (Chrome may save a font as `.sfnt`).

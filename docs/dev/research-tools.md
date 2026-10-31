# Research tools

Reverse-engineering and research tools on the Windows dev PC – analysis, function coverage, video research. Build, install and test scripts: [workflow.md](workflow.md); findings: [docs/re/](../re/README.md).

## Analysis tools and downloads

- **Hardware:** GTX 1050 Ti (DXGI adapter 1) + Intel UHD 630 (adapter 2), two 2560×1440 monitors.
- **Cheat Engine 7.7** `C:\Program Files\Cheat Engine`; **x32dbg** (winget alias `x32dbg`). **dumpbin** (MSVC): `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86\dumpbin.exe` – from Git Bash its `/switches` are turned into paths, run it from PowerShell.
- **Ghidra 12.1.4** (`C:\tools\ghidra_12.1.4_PUBLIC`, JDK Temurin 21), headless: `support\analyzeHeadless.bat <project dir> <name> -import <file>` (later `-process <file> -noanalysis -readOnly`) `-scriptPath <dir> -postScript <Script>.java …` – project, scripts and output only in the scratchpad.
- **Video research** (results and media only in the scratchpad, summaries in own words): **yt-dlp 2026.10.26** `C:\tools\yt-dlp\yt-dlp.exe` – captions, description, chapters, comments without the video: `--skip-download --write-subs --write-auto-subs --sub-langs "en.*,de.*" --write-description --write-info-json --write-comments`; **ffmpeg 9.0** (BtbN build) `C:\tools\ffmpeg-n9.0\ffmpeg-n9.0-latest-win64-gpl-9.0\bin` – frames at scene changes, audio extraction; **faster-whisper 1.2.1** in the venv `C:\tools\whisper-venv` (CPU, int8, model `large-v3-turbo` loaded from Hugging Face on first use) – only for videos without captions, with the VAD filter (it invents text over silence and music). yt-dlp pitfalls: fetching comments and captions of several videos in one run gets HTTP 429 from YouTube – fetch captions per video with `--sleep-subtitles 10` and a pause between videos; it warns that no JavaScript runtime (deno) is installed – captions and metadata still work; the `en` track of a German video is a machine translation, take `de-orig`.
- **`C:\dev\downloads`** (verified, outside the repo): `patch134_en\files`, `dgvoodoo2_87_5`, `dgvoodoo2_87_3`, `ultimate-asi-loader_9.7.4\dinput8.dll`; versions and SHA-256 in `docs/re/environment.md`.

## Function coverage (`ddmod_coverage`, `docs/re/coverage.md`)

`build/Release/ddmod_coverage.asi` plus `ddmod_coverage.in` (line `timestamp <PE TimeDateStamp hex>`, optional line `defer-window`, then one function address per line, made from the Ghidra export in the scratchpad) into a working copy. Without `defer-window` it measures from the start, but `RivellonRenewed.asi` must be renamed away (its scans read the patched bytes); with `defer-window` it starts at the game window and runs beside the mod. Result per start `ddmod_coverage-<start time>.out` (address, ms of first hit) – copy them out, then remove the tool's files.

**Play copy `dd-play`:** `LogLevel=info`, `ddmod_coverage.asi` with `defer-window` always on – collect its `ddmod_coverage-*.out` from time to time.

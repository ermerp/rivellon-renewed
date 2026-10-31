# Changelog

## Unreleased

Nothing yet.

## v0.01 – 2026-10-30

First release. Its features: [README.md](README.md#features-v001), in detail with their settings: [docs/features.md](docs/features.md).

## Steps

One line per finished step, newest first (`docs/test-results.md` for the tests).

| Date | Step |
|---|---|
| 2026-10-31 | Dev tools: CMake 4.4.5 (v0.01 rebuilt byte-identical), Ghidra 12.2 with JDK 25 |
| 2026-10-31 | `/release` also reports updates of the dev tools (table in `docs/re/environment.md`, GitHub CLI added) |
| 2026-10-31 | Knowledge base: "Touches" column and "Other sources" in `docs/re/README.md`, conflict check in the feature workflow |
| 2026-10-31 | Docs split by the size rule: research tools into `docs/dev/research-tools.md`, runtime coverage into `docs/re/coverage.md` |
| 2026-10-31 | Research after v0.01: how the hero's sight is built and that it also gates click targeting (`docs/re/visibility.md`), the `mplayer` block is the single-player command bus (`docs/re/engine-map.md`), video research tools (yt-dlp, FFmpeg, faster-whisper) |
| 2026-10-31 | Release publishing by script: `Publish-Release.ps1` tags the tested zip's commit and creates the GitHub release with notes from `CHANGELOG.md`; README badges in the banner's colors |
| 2026-10-31 | v0.01 released: repo public, tag `v0.01` on `c43e512`, GitHub release with `RivellonRenewed-0.01.zip` and its SHA-256; README badges (latest tag, release date, commits since release) |
| 2026-10-30 | v0.01 step 7.6: release checks (versions kept, coverage and docs reports), secret scan and history rewrite before going public (T1.39-W); release zip `RivellonRenewed-0.01.zip` – same files as rc1 |
| 2026-10-30 | v0.01 step 7.5: release test of rc1 by the human passed on Windows and the Deck (T1.38-W/-D); README Deck installation clarified, known issue for the paused new-game start; `/release` and wrap-up check test coverage |
| 2026-10-30 | Automated tests in borderless full screen by default (`-Windowed` for the old way, all-black shots kept); release check of rc1 passed in DE and EN incl. a new-game start (T1.37-W) |
| 2026-10-30 | v0.01 step 7.4: README for players (install/uninstall on Windows and Deck, known issues, thanks, AI note), banner, `THIRD-PARTY-NOTICES.md`, `docs/features.md`, `docs/how-the-game-works.md` |
| 2026-10-30 | Release zip ships the MinHook license (BSD-2 notice) (T1.36-W) |
| 2026-10-30 | Original menu face identified as Lombardic (Marty Snyder's shareware copy of Goudy's capitals) – not usable (license) |
| 2026-10-30 | Reproducible builds: the same commit gives a byte-identical release zip (T1.35-W) |
| 2026-10-30 | v0.01 step 7.3: release script – clean build from a commit, ASI loader and its license fetched and SHA-256 checked, zip with its SHA-256 (T1.34-W) |
| 2026-10-30 | v0.01 step 7.2 done: installation without settings – setup by double click passed (T1.32-W); black screen at a new game not reproduced (T1.33-W) |
| 2026-10-25 | Setup script: downloads the pinned dgVoodoo2 (progress, SHA-256 check) and uninstalls the mod; full-screen movies off by default (T1.31-W) |
| 2026-10-25 | Resolution picked by the mod, full-screen movies, dgVoodoo aspect scaling; fresh Steam install analyzed (T1.28-W, T1.29-W, T1.30-D) |
| 2026-10-25 | Play copy `dd-play` updated to the T1.27 build |
| 2026-10-25 | v0.01 step 7.1: docs cleanup – small session context, one place per fact, old full texts in `docs/archive/` |
| 2026-10-25 | Phase 1 merged into `main`; release path for v0.01 planned |
| 2026-10-25 | Dialog and overlay in the bundled DejaVu Sans Condensed Bold, no system fonts (T1.27) |
| 2026-10-25 | Plan B accepted on the Deck after a clean install (T1.26-D) – Phase 1 step 7 done |
| 2026-10-24 | Own rasterizer for the bundled faces: stb_truetype with GDI's sizing and bold repeated, `[Fonts] Raster` (T1.25-W) |
| 2026-10-24 | Deck test of Phase 1 (T1.24-D): mod runs under Proton; faces narrower through Wine's bold → plan B |
| 2026-10-24 | Bug round: own key repeat for text fields (T1.23-W), Ctrl+Shift+F2 with a potion on F2 (T1.22-W), black screen not reproduced (T1.21-W) |
| 2026-10-24 | Borderless full screen via dgVoodoo, F10/Alt menu-key fix (T1.20-W) |
| 2026-10-24 | Walk radius as fixed pixels and double click run, movement RE (T1.19-W) |
| 2026-10-23 | Own option arrows, "v0.01" banner, fog switch, short menu labels (T1.18-W) |
| 2026-10-23 | Play sessions in `dd-play`: coverage 55 % of the game code, play-test log, sight-range RE (TRE.1-W) |
| 2026-10-23 | Engine map of `div.exe`, function coverage tool, play copy `dd-play` |
| 2026-10-23 | Glyph renderer R5: capitals matched on all letters, ink below the line top, banner in the game's look (T1.17-W) |
| 2026-10-23 | Glyph renderer R4: menu arrow glyphs kept, remaining screens (T1.16-W) |
| 2026-10-23 | Glyph renderer R3: file behind each face logged, FR/RU spot test (T1.15-W) |
| 2026-10-23 | Glyph renderer R2: faces per family in the mod (T1.14-W) |
| 2026-10-23 | Glyph renderer R1: shared text style renderer `src/text`, version 0.01 |
| 2026-10-22 | Own glyph tables for the bitmap fonts: analysis and proof of concept (T1.13-W) |
| 2026-10-22 | Font choice per family from comparison sheets in the game's look, measured styles |
| 2026-10-21 | Overlay start banner, FPS off by default with a toggle hotkey (T1.12-W) |
| 2026-10-20 | dgVoodoo watermark switch (T1.11-W) |
| 2026-10-20 | Renamed to Rivellon Renewed (T1.10-W) |
| 2026-10-20 | Font mode: original text colors (T1.9-W), Backspace and letter-spacing fixes, input script for test runs, log history |
| 2026-10-19 | Language check FR/RU, charset setting, Russian edition best effort (T1.8-W) |
| 2026-10-19 | Font mode: `use8bitfont 0` in memory (T1.7-W) |
| 2026-10-18 | Font hook for game texts and shared font chain; font families identified, `ddmod_fontpreview` |
| 2026-10-18 | Phase 1 step 6: minimal overlay via `CD3D6VideoMem` at `DllSlashedEndFrame` (T1.6-W) |
| 2026-10-18 | Phase 1 step 5: deferred start, `WH_GETMESSAGE` input hook, `[Keys]` hotkeys (T1.5-W) |
| 2026-10-18 | Phase 1 step 4: signature scanner, hook manager, EXE hash check (T1.4-W) |
| 2026-10-18 | Phase 1 steps 1–3: ASI plugin, log, INI (T1.1-W) |
| 2026-10-18 | Phase 0 done: target 1.0062A, overlay plan, dgVoodoo2 on Windows only (T0.6-W, T0.6-D) |
| 2026-10-17 | Phase 0 started: repo, rules, import analysis, patch 1.34A contents |

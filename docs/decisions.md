# Decisions

One line per decision with its reason; the human keeps these decisions (CLAUDE.md). Rejected approaches: [rejected.md](rejected.md).

## Project and release

- **Name "Rivellon Renewed", file stem `RivellonRenewed`** (`.asi`, `.ini`, `.log`, `.prev.log` in the game folder, everything else in `RivellonRenewed\`) – the full name stands out among hundreds of game files; one folder keeps the game folder tidy. Internal identifiers (`namespace ddmod`, CMake targets, `Install-DdMod`) and `C:\dev\dd-modern` stay. No Larian logo; README disclaimer "Not affiliated with or endorsed by Larian Studios".
- **First release v0.01** (`MOD_VERSION` `0.01` in `CMakeLists.txt`; banner "v0.01", zip `RivellonRenewed-0.01.zip`).
- **MIT license for our code, copyright holder `ermerp`** – usual for mods, fits every bundled component (OFL/DejaVu ship with their license files, MinHook BSD-2 needs its notice); the pseudonym keeps the human's name out of the repo.
- **Commits under the GitHub noreply address**; user paths and LAN IPs stay out of the repo (Deck IP in Claude's local memory).
- **Before going public, the history keeps its commits; only files that now live in `local/` (plan, roadmap, test instructions, play-test log, open questions) and two placeholder folders of planned tools are removed from every commit** – the secret scan found no private data, so old texts about plans in other files may stay; squashing everything into one commit would lose the history behind the docs (human's decision, variant A).
- **`/release` also reports updates of the dev tools** – not only shipped components; a dev tool update needs no player test, only the human's OK (human, 2026-10-31).
- **Colors of new visible elements come from the game itself** (e.g. its red/white distance highlight), never an arbitrary shade – rule 12 (human's rule, 2026-10-31).
- **Video research tools yt-dlp, FFmpeg, faster-whisper (CPU, model `large-v3-turbo`, plain pip install without pinned hashes)** – captions first, Whisper only without captions; the human chose all three and the plain pip install.
- **Branches: one per feature or fix, merged into `main` with a merge commit after its test, then deleted; a release is a tag on `main`** – simple for one person; `main` may be ahead of the latest release (`CHANGELOG.md` "Unreleased"), users install from Releases. Pushed branches may stay visible on GitHub (hiding them is not worth it).
- **GitHub page like `ermerp/masterthesis`:** badges latest tag, release date, commits since latest release (Shields.io, colors from the banner: label `2b2b2b`, value `8a7f5f`); releases in the sidebar; short README, details as Markdown sub-pages in `docs/`.
- **README: feature list for players in `README.md`, details with their settings in `docs/features.md`, the game's internals for readers in `docs/how-the-game-works.md`; `CHANGELOG.md` "Unreleased" points there for v0.01** – players look for features in the README, one place per fact (human's choice).
- **Banner "RIVELLON / RENEWED" in two lines, Uncial Antiqua in the main menu's capital color (`tools/banner/New-Banner.ps1`): in the README 1280×400 with a transparent background, as social preview 1280×640 on dark stone** – closest to the game's menu; the black edge and shadow keep it readable in GitHub's light and dark theme, the transparent background fits both (human's choice, rule 12); `docs/images/` is the only place for images in the repo, our own renderings only.
- **Release zip built only by `tools/release/New-Release.ps1` from a commit, the ASI loader fetched from its release, our `LICENSE` shipped as `RivellonRenewed\licenses\RivellonRenewed.txt`** – no local copies or uncommitted files can slip in; the loader zip has no license file, so its MIT text comes from the tag's commit; shipping our own license next to the binaries is common practice and shows players the terms (human's OK).
- **Releases published locally by `tools/release/Publish-Release.ps1` (tag + `gh release create`), not by GitHub Actions** – the published zip is the tested one; a CI build in another folder gives a different `.asi` (reproducible only in the same folder) and third-party actions would need pinning (human's choice).
- **Reproducible builds (`/Brepro`)** – the same commit gives the same `RivellonRenewed.asi` and zip when built in the same folder (`build\dist`, `docs/dev/workflow.md`), so anyone can check that a release matches its source (practice of Chromium, Debian, Tor); worth it for an unsigned DLL in the game folder, costs two lines (Claude's decision, delegated by the human).
- **Credit everything used from others, also inspiration, in `docs/credits.md` (CLAUDE.md rule 14)** – fair to the community and the README thanks need a source; noted at the time of use, because it is forgotten later (human's request).
- **Every release gets a final run with the release build** – the human's release test plus one start of each earlier automated check (e.g. one new-game start for T1.21-W), not the full matrix.
- **Docs hygiene check before every release, as a report** (skill `/release`) – rules and docs drift between releases; the human decides what changes (human's rule).
- **Test coverage check before every release (`/release`) and a test look at every wrap-up with a new or changed feature** – tests only grew when someone remembered; a look at the files, no game start (human's rule).
- **A README-only fix needs no new release candidate** – the README is not in the release zip, so the tested zip stays the same (rc1 of v0.01, human's decision).
- **Third-party versions checked before every release (skill `/release`)** – a newer version of dgVoodoo2, the ASI loader, MinHook, stb_truetype or a font is pinned only after the candidate passes its tests with it (human's request).
- **A release ships the third-party versions its release test ran with; newer ones come with the next candidate** – an update changes the zip (`setup/setup.ps1` pins dgVoodoo2 and the loader), so it would need a new candidate and the human's test again; v0.01 keeps dgVoodoo2 2.87.5 and the ASI loader 9.7.4 (2.87.6 and 10.0.2 were days old, 10.x a rewrite; human's decision).
- **`CLAUDE.md` under ~150 lines / ~12 KB, release checks in a skill** – it is loaded whole into every session; Anthropic's guidance (code.claude.com/docs/en/memory) targets under 200 lines because long files cost context and weaken adherence; a skill loads only when needed. The earlier ~8 KB had no recorded reason and was too tight.

## Direction

- **The game in 16:9 and 16:10, filling the screen** – the Deck is 1280×800 (16:10); it runs the game at 1280×800, not 1280×720 with bars; 4:3 is secondary.
- **Replacements are clean-room** – understand with the decompiler (scratchpad only), document in own words, own implementation, comparison test, switchable hook (rule 2). Fonts were the first (~90 functions, 22 entry points, pixel-exact comparison, no game state; `docs/re/engine-map.md`).
- **Knowledge base: our verified findings in `docs/re/`, other people's knowledge bases as one line each, a conflict check before every feature** – OpenDivine and divproxy already cover our exact EXE, so we verify and write only what a feature needs instead of building a second KB; the "Touches" column lets a search find every file about a mechanic (e.g. the sight fix also touches click targeting) (human, 2026-10-31).
- **No system dependencies (rule 13)** – bundled resources and own code, so one build looks the same on Windows and Proton; unavoidable system parts behind a thin hook layer.

## Platforms and versions

- **Resolution by the mod: `[Display] Resolution=auto|config|<w>x<h>`, auto = the largest display mode of the primary monitor up to `AutoMaxHeight=1080` lines, the monitor's aspect ratio first; set in memory only, `config.div` untouched** – installation needs no settings (a fresh Steam install starts at 1024×768); the game does not scale, so above 1080 lines figures and text get small (the human plays 1920×1080 on 2560×1440); uninstalling leaves the old resolution. Working copies use `AutoMaxHeight=0`, since work at higher resolutions is the goal.
- **Full-screen movies (`[Display] FullScreenMovies`, off by default since the desktop flashes during the mode switch): `resolution switch` set in memory and every movie call allowed to switch to 640×480 and back; status line hidden while a movie plays; dgVoodoo `ScalingMode = stretched_ar` (`[DgVoodoo] Scaling=aspect`)** – Steam lacks the one movie that may switch, so the intro stayed a small box at high resolutions; aspect scaling keeps 4:3 movies undistorted with side bars and changes nothing for 16:9 game resolutions (human's request, T1.29-W).
- **dgVoodoo2 by a setup script, not in the zip; the ASI loader in the zip** – the dgVoodoo2 package carries no license that allows redistribution, the loader is MIT; `RivellonRenewed-Setup.cmd` downloads the pinned version from the official release with visible progress and SHA-256 checks, and also uninstalls (human: the script must run visibly with progress).
- **Target EXE 1.0062A** (Steam/GOG) – everyone has it, handles 2560×1440; 1.0034A needs a CD-path workaround (`docs/re/patch-134.md`).
- **Language priority: 1. EN + DE, 2. FR, 3. RU** – EN/DE fully tested; FR same build, spot test per release (accents); RU (own EXE 1.0061) best effort: overlay and input run, original Cyrillic bitmap fonts, `[Fonts] Charset=auto` picks 1251 by EXE hash.
- **Proxy: Ultimate ASI Loader as `winmm.dll`** – `div.exe` imports it; `ddraw.dll`/`D3DImm.dll` belong to dgVoodoo2; coexists with OsirisRE's `osi_extender`.
- **dgVoodoo2 on Windows only** – it crashes under Proton (T0.6-D).
- **Every phase tested in `dd-test` (DE) and `dd-test-en` (EN)** – same `div.exe`, different language files.
- **Separate play copy `C:\dev\dd-play` with permanent coverage monitoring** – the human's real saves stay apart from test saves and automated runs; every session adds runtime data.

## Plugin core

- **MinHook v1.3.4 via FetchContent (pinned commit), own CMake target** – same static CRT/x86 settings as the plugin.
- **Own SHA-256 instead of bcrypt** – hashing runs in `DllMain`, where loading `bcrypt.dll` under the loader lock is unsafe.
- **Deferred start via a watcher thread** – hooks go in once the window of class `Divinity` exists (loader lock; the renderer DLL is not loaded before).
- **Log history:** previous start as `RivellonRenewed.prev.log`, older ones in `RivellonRenewed\logs\` by start time (`[General] LogHistory`, default 20) – testers restart often and report by screenshot time; the start time comes from the log's first line (NTFS keeps old creation times).

## Input and movement

- **Hotkeys via a `WH_GETMESSAGE` hook on the game thread** – independent of who owns the window procedure (dgVoodoo2, WineD3D); swallowed keys become `WM_NULL`.
- **No F10 and no Alt combinations as default hotkeys** – a missed key puts the game into Windows' menu mode.
- **F10 and a lone Alt reach the game as normal keys (`[Features] BlockMenuKey=1`)** – they put a framed window into Windows' menu loop (the "F10 softlock"); rewriting keeps the key for the game.
- **Own key repeat for Backspace, Delete, Left, Right in text fields (`[Features] KeyRepeat=1`)** – human's wish; the text fields act on key-up, so the hook posts one key-up at once and more from a timer (no memory access); `[Input] RepeatDelay=300` ms, `RepeatRate=15`/s – Windows' 500 ms / 30/s felt wrong.
- **Walk/run circle as a fixed pixel radius, default 225 px at every resolution (`[Movement] WalkRadius`, `original` = 3/8 of the screen height)** – the world is drawn 1:1, so pixels mean the same world distance everywhere; value from the human's feel test (T1.19-W); set in the INI only.
- **Double click = run for click moves (`[Movement] DoubleClickRun`, default on)** – a single click always walks in the original.
- **The hero stops when a held button is released (as in the original)** – the human finds it right.

## Window and graphics

- **Borderless full screen through dgVoodoo2's `FullscreenAttributes = fake`, set by the mod, default on (`[DgVoodoo] FullScreen=borderless`)** – the human plays full screen and wants Alt+Tab; dgVoodoo already does it, no window code of our own.
- **dgVoodoo's watermark switched by the mod (`[DgVoodoo] Watermark`, default off)** – rewriting one key in `dgVoodoo.conf` at every start is simpler than intercepting its config reads; `keep` leaves the file alone.
- **Windows full-screen optimizations: README note only, no compatibility flag** – the reported crashes did not show in nine runs (T1.20-W); a registry change by the installer is more invasive.
- **Fog on by default, the INI is the master (`[Features] Fog=1|0|keep`)** – Steam's `config.div` switches the ground fog off with `no fog`; the game's performance page writes its choice back into the INI.
- **Overlay base backend: DirectDraw/D3D6 at `DllSlashedEndFrame`** – the only path that works with Windows ddraw, dgVoodoo2 and WineD3D.
- **Overlay text: own glyph blitter through the game's `CD3D6VideoMem`** (`DllSlashedGetInfo(3)`) – repeats the mid-scene lock/write/unlock the game does every frame, so the path is proven on all platforms and pixel formats and allows alpha blending.
- **Overlay for v0.01: start banner for ~5 s, FPS off by default** (`[Overlay] Status=banner|always|off`, `ShowFps`, `[Keys] ToggleFps` Ctrl+Shift+F2) – the game reads the screen for a save's preview picture, so a permanent overlay ends up in every save. README known issue: FPS can appear in save pictures.
- **Start banner in the game's look, top left at fixed pixel sizes (`[Overlay] BannerLook=game`)** – "RIVELLON RENEWED" in the menu family at `fontbig24`'s size, "v0.01" in the load list's white (`fontn2`), "ACTIVE" in the load title's green (`fontgr1`), pen x 18, baseline y 38; capitals because Uncial Antiqua's lowercase misreads. Human's choice (variant 1, `t1.17\03`).
- **Own option arrows: plain triangles in the original's box and measured style (variant A)** – human's choice from five shapes; smooth like the original, no menu gloss.
- **Workaround `[Fonts] ShortLabels` (DE "HINTERGRUNDGERÄUSCHE" → "HINTERGRUND")** – the only label over an arrow.

## Text and fonts

In [decisions-fonts.md](decisions-fonts.md).

## Testing

- **Automated walkthroughs by real input (`Start-GameTest.ps1 -Steps`) with known sequences** – only when the human has said the PC is free.
- **`Start-GameTest.ps1` runs in full screen by default (`-Windowed` for the old way), all-black shots are kept and marked `-BLACK`** – players run borderless full screen, and a real black screen must show up in the results instead of being skipped.
- **`Start-GameTest.ps1 -Intro`** – the EN copy ships `startup logo 0`; DE and EN must run under the same conditions.
- **Resolution tests set `config.div` and offsets 0x0C/0x10 of `slashed-d3d6.cfg` directly** instead of running `configtool.exe` – scriptable, same effect.

# Credits and sources

Everything from others that Rivellon Renewed uses – code, data, tools, knowledge, ideas, also mere inspiration – with who made it and where it is. A line is added the moment we use something (CLAUDE.md rule 14), so the README can credit it. Pinned versions: `docs/re/environment.md`.

## Shipped with the mod or fetched by it

- **Ultimate ASI Loader** – ThirteenAG, `github.com/ThirteenAG/Ultimate-ASI-Loader` (MIT); loads `RivellonRenewed.asi`, shipped as `winmm.dll`.
- **dgVoodoo2** – Dege, `github.com/dege-diosg/dgVoodoo2`; DirectX wrapper for borderless full screen and scaling, downloaded by the setup.
- **MinHook** – Tsuda Kageyu, `github.com/TsudaKageyu/minhook` (BSD-2); function hooks.
- **stb_truetype** – Sean Barrett, `github.com/nothings/stb` (public domain / MIT); outlines of the bundled faces.
- **Fonts** (via `github.com/google/fonts`, OFL): Uncial Antiqua, Fondamento and Marcellus SC by Brian J. Bonislawsky (Astigmatic); Almendra SC by Ana Sanfelippo. **DejaVu Sans Condensed Bold** – DejaVu fonts team, `dejavu-fonts.github.io` (Bitstream Vera license).

## Knowledge and leads

- **atom0s** (Steam forum, 2020): the one-byte developer-mode patch of `div.exe` 1.0062A – the lead for `docs/re/developer-mode.md` (we never patch the EXE).
- **Steam forum** (author not recorded): the name "divineevent.dat" for the key event file (`docs/re/key-events.md`).
- **Raanar**, Divinity_Graphics_Extender (`github.com/Raan/Divinity_Graphics_Extender`, README only): sprite packs are decodable and re-encodable, size fixed on import – shaped the plan for HD sprites (`docs/re/file-formats.md`).
- **Divinity Wiki** (Fandom, `divinity.fandom.com`): its heading font, from Larian's Divinity II fansite kit, identified the original menu face as Lombardic; **DaFont** (`dafont.com/lombardic.font`) holds the original package with the author's readme – the license terms (`docs/re/fonts-mod.md`).
- **usernameak**, OsirisRE (`github.com/usernameak/OsirisRE`): Osiris notes and `osi_extender` as `.asi` – the mod is built to coexist with it (`docs/decisions.md`).

## Tools used in development

Ghidra (NSA), x32dbg, Cheat Engine, CMake, MSVC build tools, 7-Zip; Claude Code (Anthropic) as development assistant – its memory guidance (`code.claude.com/docs/en/memory`) set the `CLAUDE.md` size limit.

## Game

Divine Divinity by Larian Studios – the mod changes nothing in the game's files and ships none of them; not affiliated with or endorsed by Larian Studios.

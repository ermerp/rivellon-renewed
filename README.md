# Rivellon Renewed

*A modernization mod for Divine Divinity* – unofficial, for the Steam/GOG version 1.0062A of **Divine Divinity** (Larian Studios, 2002).

> Status: in development (Phase 1 – foundation: plugin, overlay, game texts in a scalable font). No release yet.

Planned: quality-of-life features, better readability at high resolutions, WASD and controller support, audio logs for books and letters – while keeping the game's original look. See [PLAN.md](PLAN.md) for details.

## Building (developers)

Requirements: Visual Studio Build Tools 2022 (MSVC, x86), CMake ≥ 3.24.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

Output: `build/Release/RivellonRenewed.asi`. Configuring fetches [MinHook](https://github.com/TsudaKageyu/minhook) v1.3.4 (pinned commit) via CMake `FetchContent`. Unit tests (no game needed): `ctest -C Release --test-dir build --output-on-failure`. It is loaded by [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) installed as `winmm.dll` in the game folder, next to `RivellonRenewed.asi` and `RivellonRenewed.ini`.

## Notice

Not affiliated with or endorsed by Larian Studios. Divine Divinity is a trademark of Larian Studios.

This project contains **no game files** and requires a legally purchased copy of Divine Divinity.

## License

Own code under the [MIT License](LICENSE).

Third-party code: [MinHook](https://github.com/TsudaKageyu/minhook) by Tsuda Kageyu (BSD 2-Clause License), statically linked into `RivellonRenewed.asi`; its license text must ship with binary releases.

Bundled font: DejaVu Sans Condensed Bold from [DejaVu Fonts](https://dejavu-fonts.github.io/) 2.37 (`assets/fonts/`, installed as `RivellonRenewed\fonts\`), under the Bitstream Vera / DejaVu license in `assets/fonts/LICENSE-DejaVu.txt`. It is only a fallback: the mod uses the system font Tahoma when it is available (`[Fonts] Chain` in `RivellonRenewed.ini`).

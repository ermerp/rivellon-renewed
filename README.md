# dd-modern

Unofficial modernization mod for **Divine Divinity** (Larian Studios, 2002), Steam/GOG version 1.0062A.

> Status: in development (Phase 0 – setup and inventory). Nothing installable yet.

Planned: quality-of-life features, better readability at high resolutions, WASD and controller support, audio logs for books and letters. See [PLAN.md](PLAN.md) for details.

## Building (developers)

Requirements: Visual Studio Build Tools 2022 (MSVC, x86), CMake ≥ 3.24.

```
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
```

Output: `build/Release/ddmod.asi`. Configuring fetches [MinHook](https://github.com/TsudaKageyu/minhook) v1.3.4 (pinned commit) via CMake `FetchContent`. Unit tests (no game needed): `ctest -C Release --test-dir build --output-on-failure`. It is loaded by [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) installed as `winmm.dll` in the game folder, next to `ddmod.asi` and `ddmod.ini`.

## Notice

This project contains **no game files** and requires a legally purchased copy of Divine Divinity. It is not affiliated with Larian Studios.

## License

Own code under the [MIT License](LICENSE).

Third-party code: [MinHook](https://github.com/TsudaKageyu/minhook) by Tsuda Kageyu (BSD 2-Clause License), statically linked into `ddmod.asi`; its license text must ship with binary releases.

Bundled font: DejaVu Sans Condensed Bold from [DejaVu Fonts](https://dejavu-fonts.github.io/) 2.37 (`assets/fonts/`, installed as `ddmod_fonts\`), under the Bitstream Vera / DejaVu license in `assets/fonts/LICENSE-DejaVu.txt`. It is only a fallback: ddmod uses the system font Tahoma when it is available (`[Fonts] Chain` in `ddmod.ini`).

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

Output: `build/Release/ddmod.asi`. It is loaded by [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) installed as `winmm.dll` in the game folder, next to `ddmod.asi` and `ddmod.ini`.

## Notice

This project contains **no game files** and requires a legally purchased copy of Divine Divinity. It is not affiliated with Larian Studios.

## License

Own code under the [MIT License](LICENSE).

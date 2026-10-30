# How Divine Divinity works – what we learned

A readable tour of what building this mod taught us about the game's 1.0062A `div.exe`. Every point links to the detailed notes in [docs/re/](re/README.md); why the mod does things the way it does is in [decisions.md](decisions.md), approaches that did not work in [rejected.md](rejected.md), and the incidents behind our ways of working in [lessons.md](lessons.md). All findings are written in our own words; the mod contains no code of the game.

## The program

- `div.exe` holds about 7 900 functions of game code (about 1.9 MB of machine code) and 568 C++ classes. It still carries the paths of 200 of Larian's source files, which let us map its code to subsystems such as agents, AI, the GUI and the world – [engine-map.md](re/engine-map.md), [engine-classes.md](re/engine-classes.md).
- Sound runs through FMOD 3, movies through Bink, scripted story logic through Larian's Osiris engine (`OsirisDLL.dll`), dialogs through `DivDialogSystem.dll` – [architecture.md](re/architecture.md).
- The Steam version 1.0062A is built on the 1.0029A code base, not on the last patch 1.0034A, whose source was lost; their savegames are not compatible. The Russian Steam edition is a different build, 1.0061 – [game-version.md](re/game-version.md).

## The picture

- The game does not draw through Windows directly: it loads a renderer plugin, `slash1.dll` (Direct3D 6) to `slash4.dll`, which creates the game window and draws every frame. The mod draws its banner just before the renderer shows a finished frame – [renderer-d3d6.md](re/renderer-d3d6.md), [overlay.md](re/overlay.md).
- The resolution comes from `config.div`: the game copies it into two global values that more than a hundred places read. In full screen the size must be one the renderer offers – [config-files.md](re/config-files.md).
- The game never scales its picture. On a large screen you see more of the world, but persons farther from the hero than his sight range fade out although the ground around them is drawn – the range was made for 800×600 screens – [visibility.md](re/visibility.md).
- Sprites are 16-bit color and packed in the game's own formats – [graphics-assets.md](re/graphics-assets.md), [file-formats.md](re/file-formats.md).

## Text

- All texts come from 61 bitmap fonts in the `fonts` folder. A hidden mode (`use8bitfont 0`) replaces all of them with Windows fonts – every one as "SimSun", a Chinese system font; the mode was evidently made for the Chinese version – [text-rendering.md](re/text-rendering.md).
- The game cuts off anything that sticks out above a line's top, which is why umlaut dots and accents need care in the menu faces – [glyph-layout.md](re/glyph-layout.md). How the mod draws its letters: [fonts-mod.md](re/fonts-mod.md), measured against Windows' font engine in [gdi-raster.md](re/gdi-raster.md).

## Input and movement

- The game reads keys from its own message loop without DirectInput, and text fields act when a key is released, not when it is pressed – [input.md](re/input.md). Key bindings live in `keylist.txt` – [key-events.md](re/key-events.md).
- Holding the mouse button makes the hero run outside a circle of 3/8 of the screen height around the screen center – [movement.md](re/movement.md).

## Configuration

- `config.lcl` holds installation and language, `config.div` the display and user settings; the game rewrites `config.div` on exit. After a crash it asks whether to reset the configuration – [config-files.md](re/config-files.md).
- A developer mode exists behind a single flag – [developer-mode.md](re/developer-mode.md).

## Running it today

- dgVoodoo2 turns the game's DirectDraw/Direct3D 6 into Direct3D 11 on Windows and gives a borderless full screen; under Proton on the Steam Deck it crashes, so the Deck runs without it – [environment.md](re/environment.md).

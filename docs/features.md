# Features and settings

What each feature of Rivellon Renewed does, where it works, and which settings in `RivellonRenewed.ini` change it. The short list is in the [README](../README.md#features-v001); every setting is explained by the comment above it in `RivellonRenewed.ini` (also in [config/RivellonRenewed.ini](../config/RivellonRenewed.ini)). Change the file while the game is not running.

## Text in vector faces

The game draws its texts with bitmap letters made for 800×600; at today's resolutions they are tiny and hard to read. The mod gives the game's own font objects new letters drawn from vector faces at the size the game asks for, so the layout of menus, books and dialogs stays the game's. Each kind of text keeps its character: the menu's decorated capitals in Uncial Antiqua (with the original gloss and texture), the diary title in Almendra SC, the interface in Marcellus SC, books and letters in Fondamento (with a slightly wider line pitch, pages re-flow), NPC dialogs in DejaVu Sans Condensed Bold. The letters are drawn by the mod's own rasterizer from the bundled fonts, so Windows and the Steam Deck show the same pixels; no installed system font is used.

- Off: `[Features] FontHook=0` (all original letters) or `Enabled=0` in one `[Family.*]` section (only that kind of text).
- Faces, weight and line height per kind of text: `[Family.Interface]`, `[Family.Books]`, `[Family.Menu]`, `[Family.Dialog]`, `[Family.DiaryTitle]`.
- Letter spacing: `[Fonts] Spacing`. A long German menu label is shortened so it does not run over the arrows: `[Fonts] ShortLabels`.
- The Russian edition keeps its original Cyrillic letters.

## Resolution without a configuration tool

The game normally runs in the size set with its configuration tool (`configtool.exe`), which offers only a few old sizes. The mod picks the largest display mode of the main monitor up to 1080 lines in the monitor's aspect ratio (on the Steam Deck 1280×800) and tells the game at start-up; `config.div` is never changed. The game does not scale its picture: a higher resolution shows more of the world with smaller figures, which is why the default stops at 1080 lines.

- `[Display] Resolution`: `auto`, `config` (as the configuration tool set it) or a size such as `1920x1080`. `[Display] AutoMaxHeight`: the limit for `auto` (`0` = the monitor's own resolution).

## Full-screen movies

The intro and story movies are 640×480 and normally play small in the middle of the screen. With `[Display] FullScreenMovies=1` the game switches to 640×480 while a movie plays, so it fills the screen (4:3 with black bars at the sides), and back afterwards. Off by default: the desktop can flash during the switch.

## Borderless full screen (Windows, dgVoodoo2)

With [dgVoodoo2](https://github.com/dege-diosg/dgVoodoo2) installed (the setup does it), the game runs in a frameless window the size of the screen: Alt+Tab switches fast and the game comes back in full screen. The mod sets three dgVoodoo2 options in `dgVoodoo.conf` at every start; nothing else in that file is touched. On the Steam Deck dgVoodoo2 is not used.

- `[DgVoodoo] FullScreen`: `borderless` or `exclusive`. `[DgVoodoo] Scaling`: `aspect` or `stretch`. `[DgVoodoo] Watermark`: dgVoodoo2's logo in the corner, off by default. `keep` leaves an option to `dgVoodooCpl.exe`.

## F10 and Alt no longer freeze the game

In a game window with a frame (windowed, or after Alt+Tab from exclusive full screen), F10 or a lone Alt press opens Windows' hidden window menu and the game stops until the next click. The mod hands both keys to the game as normal keys. Alt+Tab, Alt+F4 and Alt+Enter keep working.

- `[Features] BlockMenuKey=0` turns it off.

## Key repeat in text fields

In the original, Backspace, Delete and the arrow keys in text fields (such as a save name) act only when released and only once per press. With the mod they act when pressed and repeat while held.

- `[Features] KeyRepeat`; speed: `[Input] RepeatDelay` and `RepeatRate` (or `windows` for the keyboard settings of Windows).

## Walking and running with the mouse

Holding the left mouse button makes the hero walk while the cursor is near the screen center and run beyond. The game sizes that circle by the screen height (3/8 of it), so at 1920×1080 the hero walks within 405 pixels – far more of the world than at 800×600. The mod uses a fixed 225 pixels, the game's circle at 800×600. A double click on the ground makes the hero run there; a single click still walks.

- `[Movement] WalkRadius` (`original` = the game's circle), `[Movement] DoubleClickRun`.

## Ground fog

Steam ships the game with ground fog switched off in `config.div`. The mod switches it on at every start; if you change fog in the game's picture settings, the mod stores your choice in `RivellonRenewed.ini` when the game exits.

- `[Features] Fog`: `1` on, `0` off, `keep` as `config.div` says.

## Start banner and frame rate

For five seconds after start "Rivellon Renewed v0.01 active" shows in the top-left corner – a quick check that the mod is loaded. Ctrl+Shift+F2 shows or hides the frame rate.

- `[Overlay] Status` (`banner`, `always`, `off`), `[Overlay] BannerSeconds`, `[Overlay] ShowFps`; keys in `[Keys]`.
- The overlay works with the game's Direct3D renderer (the default).

## Logs

`RivellonRenewed.log` in the game folder describes the last start, `RivellonRenewed.prev.log` the one before; older starts are kept in `RivellonRenewed\logs\`. `[General] LogLevel` sets the detail, `[General] LogHistory` how many old logs are kept.

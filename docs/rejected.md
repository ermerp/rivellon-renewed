# Rejected approaches

One line each, with why – so they are not tried again. Rejected test methods and the incidents behind rules: [lessons.md](lessons.md). Decisions: [decisions.md](decisions.md).

## Platform and plugin

- 1.0034A EXE as main target – CD prompt / `slash1.dll` crash with videos above 1024×768; on hold, not dropped (`docs/re/patch-134.md`).
- `dinput.dll` / `ddraw.dll` as proxy – the game never loads `dinput.dll`; `ddraw.dll` is taken by dgVoodoo2.
- dgVoodoo2's own `WinMM.dll` – collides with the ASI loader, not needed.
- Patching the EXE (e.g. the forum byte patch at `0x8CCD6`) – rule 3.
- `debug on` in `config.lcl` for the developer mode – ignored by 1.0062A; the flag must be set in memory (`docs/re/developer-mode.md`).
- Console ports (PlayStation, Xbox, Switch) – the mod hooks a Windows x86 EXE, consoles need devkits, certification and Larian's rights; Steam Deck and Steam Machine are our console-like platforms.

## Overlay and screen captures

- D3D11-only overlay via dgVoodoo2 – does not exist on the Deck.
- Overlay text via GDI `GetDC` on the back buffer – the game does that only for a non-16-bit back buffer, so it is unproven under dgVoodoo2/WineD3D, and GDI has no alpha (`docs/re/overlay.md`).
- The game's raw bitmap fonts as the long-term overlay font – fixed 16-bit pixel sizes.
- Hooking the four screen readers in `div.exe` (save shot, thumbnail, bug report, editor capture) to hide the overlay – more signatures, covers nothing the single `CD3D6VideoMem` `+0x30` hook would not.
- Restoring the back buffer itself under the overlay before a capture – touches the screen; patching the game's copy is enough.

## Input and window

- Subclassing the window procedure for hotkeys – in full screen dgVoodoo2 replaces it without chaining to ours (`docs/re/input.md`).
- Taking the first visible window of the process as the game window – Bink and message boxes have their own; match the class `Divinity`.
- Binding debug key events by editing `keylist.txt` – the game only exports that file (`docs/re/key-events.md`).
- Filtering `WM_CHAR` control characters in the input hook for the Backspace bug – would hide them from every game window; the glyph check is the narrower fix.
- Clearing the key-up guard `0x6496DC` before Backspace's repeats – built on the wrong idea that text fields delete on key-down; the guard was 0 anyway (`docs/re/input.md`).
- Passing Windows' auto-repeats on as key-ups – follows the Windows timing (500 ms, then 30/s), too slow to start and too fast for the human; the press still acted only on release.
- Swallowing F10 against the menu loop – the game would lose the key; it is passed on as `WM_KEYDOWN`.
- An own borderless mode (window style) or dgVoodoo's `WindowedAttributes = borderless,fullscreensize` – `FullscreenAttributes = fake` does it with one key and keeps the game's full-screen setting.

## Movement and visibility

- Walk radius as a share of the screen height or a factor on the original – grows with the resolution although the world is drawn 1:1.
- Keeping a held run going after the button is released – the human finds stopping right.
- Measuring the walk circle from the hero instead of the screen center – he is off center only at small levels' edges, Shift scrolling or screen shake; not worth a hook unless it shows up in play.
- Searching for a screen-size constant (400/512/640/800 px) behind the vanishing persons – the cut-off is the hero's sight stat (`docs/re/visibility.md`).
- The game's own `line of sight` switch (settings `+0x2C` = 0) as the visibility fix – shows persons through walls; scaling the hero's range keeps the rule.

## Fonts and text

- Bundling a free font that only roughly matches (Noto Sans, Alegreya SC, Cormorant SC, Alegreya Sans SC) – too far from the original look (rule 12).
- Mixed menu faces (roman capitals with uncial A, E, H, M, W) and Celtica, Cormorant Unicase, Montaga, Junge and others for the menu – none convinced the human more than Uncial Antiqua.
- Lombardic (Marty Snyder, probable menu source face) – shareware with a per-user fee, no permission to change it (umlauts, accents), author not reachable, no digits; our only copy comes from Larian's fansite kit (rule 1).
- Vinque (probable diary source face) – Typodermic's free license excludes games and software that include the font.
- Upscaled or smoothed original glyphs (Scale2x, xBRZ, MMPX, distance field, auto-trace) – good only up to about 2×, keep Larian's pixels.
- Making Uncial Antiqua thinner by eroding its outline – breaks its hairlines (the E's bars vanish at ¼ px); its weight-400 cut looked worse than 600.
- A fixed gloss edge (light top, dark bottom) – the original's shine is a rounding across the strokes plus texture, measured.
- Squeezing the book face's ascenders and descenders into the 12 px pitch, or setting it smaller – the human preferred Fondamento at full size; the fix is a wider pitch.
- Matching a face on its capital H alone where the H differs from the other capitals (Almendra SC, Uncial Antiqua) – the letters came out lower and smaller; median over A–Z.
- `TopFit=squeeze` as default – Uncial Antiqua's Ü dots almost vanish, d/h/k/l look stunted; `shrink` would need GDI height 14 instead of 46 for the menu.
- Finding accents by an empty row above the letter – Uncial Antiqua's Ü dots sit between the U's arms; separate connected parts instead.
- Painting the own option arrows with the menu letters' shine and texture – too light and blotchy next to the smooth originals.
- Swapping a shortened label's text pointer for an own string – the game frees the text table with its own allocator on a reload; the label is shortened in place.
- Styled game text through the GDI path (per-family `CreateFontIndirectA`, or answering `GetGlyphOutlineA` with own coverage) – the game maps coverage through a two-color ramp, so no color profile, rim, gloss or texture.
- Soft glyph edges by per-pixel alpha – the game's sprite format has none (0 = transparent, otherwise opaque); edges are blended into the family's background tone in advance.
- Passing the bitmap fonts' 8-bit colors to the game's font descriptor unchanged – the game rounds down to 565, one step darker (`docs/re/glyph-layout.md`, Colors).
- `[Fonts] Spacing=gap` as default – still left "2." and the foot in "Li" covered (the game's overlap test misses the bottom row).
- Setting `use8bitfont 0` in `config.lcl` by `Install-`/`Uninstall-DdMod` – changes a game file and needs restore logic.
- Only explaining `use8bitfont 0` in the README – a manual step for every user.
- GDI rasterizing (`GetGlyphOutline` with synthetic bold) for the bundled faces – Wine emboldens weight 600 differently (T1.24-D).
- stb_truetype for Tahoma, or FreeType only to hint Tahoma – stb cannot hint; FreeType is a large dependency for one system face.
- stb_truetype at its own exact-area coverage and fractional size – 3–29 % shape difference to GDI; whole-ppem outlines and 8×8 point sampling bring it to 0.6–2.8 %.
- Repeating GDI's `GGO_METRICS` box from the inked mask, the rounded curve extremes or a monochrome raster – only the box of all outline points (26.6, then rounded) matches.
- Repeating GDI's defect for glyphs above the ascent (a pixel lower, bottom row cut off) to keep the menu at 7 px – the human prefers the whole D at 8 px.
- Tahoma Bold as dialog/overlay face – not shippable, Wine's look-alike is thinner and wider (T1.26-D); a Proton-only default would mean two looks (rule 13).
- Checking a font chain entry by asking GDI which face it selects – GDI picks an installed font of the same name even when the file does not hold it; the file's `name` table decides.

## Config

- Leaving the fog choice to `config.div` alone – without a `no fog` line the game writes nothing back, so "fog off" is lost at the next start (also unmodded).

## Tools

- Coverage breakpoints from process start together with `RivellonRenewed.asi` – the mod's scans and MinHook read the patched entry bytes; `defer-window` starts at the game window.
- Mapping exception funclets to their parent through the unwind tables – Ghidra keeps no usable chain; the handler stub after a function's funclets is referenced by the parent's prologue (forward scan).

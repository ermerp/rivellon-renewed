# File formats

Our own descriptions of Divine Divinity's data formats.

## ML_FONT_01 (fonts)

Bitmap fonts in `fonts/*.fnt` (`use8bitfont 1`). One file per font **and color** (e.g. `dialog_white`/`_yellow`/`_red`/`_grey`, `fontn1`–`5` in five sizes, `paper_*` for books, `fontbig24/36` and `diary_head_*` with capitals and digits only). Reader: `src/overlay/fnt.cpp`; all integers little endian.

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | 10 bytes | `ML_FONT_01` |
| 0x0A | u32 | table size (256) |
| 0x0E | u32 | line height in pixels (e.g. 15 for `dialog_*`, 13 for `fontn1`) |
| 0x12 | u32 | number of glyphs in the file |
| 0x16 | 256 × i32 | glyph table indexed by character byte (code page of the language version): record offset relative to **0x26**; 0 = no glyph, negative = alias of the glyph at the absolute value |

Each glyph is a 16-byte prefix followed by the record:

- Prefix: 8 bytes `32 54 76 98 CC DD EE FF` (same in every glyph, meaning unknown), u32 0, **i32 = minus the glyph's top row** relative to the top of the line (e.g. −3 for capitals, −5 for lowercase, −11 for `.` in `dialog_white`).
- Record: u32 advance, u16 character code, u32 size (= next field + 2 × pixel count), u32 (= 12 + size of the row table), u16 width, u16 height; then one row entry per pixel row – u16 span count *n*, u32 byte offset of the row's first pixel in the pixel block, *n* × (u16 x, u16 length), u16 *n* again; then the pixel block: RGB565 values of all spans, row by row; then one padding byte.
- Pixels outside spans are transparent. The colors are pre-shaded (anti-aliased edges darkened towards black), so the glyphs are made for dark backgrounds.
- There is no space glyph (entry 0) and no space width in the header – the game's spacing is not known yet (the overlay uses the width of `i`).

## Hero sprites (`static/heroes/<class><gender><sheet>.bic|.idc`, `<class><gender>.key`)

Status: index structure understood, image data not decoded yet (2026-10-18).

- `.key` – text, CRLF. First line `Max size W,H` (largest frame). Then one line per animation: `NAME,first,end` – frame range `[first, end)` in the `.idc`/`.bic` of the sheet whose letter is the **third character** of the name (e.g. `MAC…` → sheet `C`). Names start with `M`; the second character selects the action, the fourth the variant.
- `.idc` – frame index, **40 bytes per frame**, little endian:

| Offset | Type | Meaning |
|---|---|---|
| 0x00 | u32 | offset of the frame data (in the decompressed `.bic` stream) |
| 0x04 | u32 | size of the frame data |
| 0x08 | u16 | width |
| 0x0A | u16 | height |
| 0x0C | i16 | hotspot x |
| 0x0E | i16 | hotspot y |
| 0x10 | 24 bytes | unknown; all `FF` in sheets A and E, varies in B–D (attachment points?) |

  Frames are stored back to back (offset + size = next offset). The indexed total is about twice the `.bic` file size → the `.bic` file is compressed again as a whole or in blocks.
- `.bic` – compressed frame data, 16-bit pixels with skipped transparent areas (assumed, see `graphics-assets.md`).

## Image lists (`static/imagelists/`)

Status: index entry size known only (2026-10-18).

- `CPackedi.<n>c` – index, **56 bytes per image**; starts with u32 data offset, u32 width, u32 height, followed by further fields (width-1, height-1, hotspot, …). 13 lists, 98 761 images.
- `CPackedb.<n>c` – compressed image data; begins with the same kind of stream header as the hero `.bic` files.
- `apackedi.<n>` / `APackedb.<n>` – 16-byte entries, probably animation tables; `Collide.<n>` – collision data; `roofs.000` / `roofs.dat` – roof images and their table.


# File formats

Our own descriptions of Divine Divinity's data formats.

## ML_FONT_01 (fonts)

- Offset table with 256 entries starting at byte 22; base offset 38.
- One header per glyph: advance width, character code, data size, dimensions.
- Pixels stored as RGB565, only for occupied spans.
- Negative table entries are aliases to other glyphs.

_Details will be added when `tools/extract` is built._

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


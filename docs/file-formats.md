# File formats

Our own descriptions of Divine Divinity's data formats.

## ML_FONT_01 (fonts)

- Offset table with 256 entries starting at byte 22; base offset 38.
- One header per glyph: advance width, character code, data size, dimensions.
- Pixels stored as RGB565, only for occupied spans.
- Negative table entries are aliases to other glyphs.

_Details will be added when `tools/extract` is built._

# GDI rasterizing, measured

Reference for the mod's own rasterizer (`[Fonts] Raster=own`, `src/text/ttface.*`, `fonts-mod.md`).

Measured on Windows 11 with `GetGlyphOutlineW(GGO_GRAY8_BITMAP)` against stb_truetype for the faces and GDI heights the glyph renderer uses (scratchpad spike, sheets in `C:\dev\screenshots\planb\`):

- **Synthetic bold (weight ≥ 600 on a face with only a regular cut)** is the regular glyph overstruck one pixel to the right: each output level is `min(64, regular(x) + regular(x-1))`, the black box one pixel wider, the advance one pixel larger, nothing changes vertically – bit-exact at every size (10 to 50).
- **Sizes:** a positive height is the cell height `usWinAscent + usWinDescent` (OS/2). Metrics (`tmAscent`, advances) use the scale `height / (winAscent + winDescent)` rounded to 16.16 fixed point (decides advances that end on half a pixel); the outlines are scaled to the nearest whole ppem (`round(height · unitsPerEm / (winAscent + winDescent))`).
- **Coverage:** 8×8 point samples per pixel (65 levels); emulated by rendering at 8× and counting sub-pixels at least half covered.
- **Composite glyphs** (the accented letters of the four faces): a component with `ROUND_XY_TO_GRID` is placed at its offset rounded to whole pixels; stb_truetype ignores the flag, so the mod assembles composites itself.
- **`GGO_METRICS` box** (what the capital measurements and the books' line height use): the box of all outline points, on and off the curve, each scaled down to 26.6 fixed point, then rounded to the nearest pixel – not the box of the gray bitmap.
- **GDI defect:** a glyph whose gray bitmap would reach above the face's ascent is drawn one pixel lower and loses its bottom row – in Marcellus SC at heights 12, 16 and 17 most accented capitals (À's feet, È's bottom bar; Ä Ö Ü stay below the ascent and are correct), in Uncial Antiqua at 46 the D (tip 0.5 px above the ascent) and Ð. The own rasterizer draws them whole; for the menu the D's tip then sets `TopFit=lower` to 8 px instead of 7 (human's choice).
- **Result for the four OFL faces** (unhinted: no glyph has instructions; `gasp` 0xF): ascent, cell height, advances, capital metrics and the books' ink extent equal at every height the renderer uses; shape difference 0.6–2.8 % of the ink with bold (all 1252 characters except the defect above; `ctest`, `TestTtFace`).
- **Hinted faces:** Tahoma Bold is fully hinted (stems snapped to 2 px at height 17); an unhinted rasterizer comes out softer and a pixel taller than GDI. DejaVu Sans Condensed Bold has hinting instructions too, which the own rasterizer does not run – its dialog look is the unhinted one on every platform.

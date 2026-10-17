# Dateiformate

Eigene Beschreibungen der Datenformate von Divine Divinity.

## ML_FONT_01 (Schriften)

- Offset-Tabelle mit 256 Einträgen ab Byte 22; Basis-Offset 38.
- Pro Glyph ein Header: Vorschubbreite, Zeichencode, Datengröße, Abmessungen.
- Pixel als RGB565, nur für belegte Spannen gespeichert.
- Negative Tabelleneinträge sind Aliase auf andere Glyphen.

_Details werden beim Bau von `tools/extract` ergänzt._

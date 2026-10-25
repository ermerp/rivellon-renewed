# Game version 1.0062A

Steam (and presumably GOG) ship `div.exe` FileVersion `1, 0, 0, 62` (= 1.0062A). It is based on the 1.0029A code base (the 1.0034A source was lost): widescreen and Windows 7 adjustments, but not all 1.0034A fixes. Savegames are not compatible with 1.0034A (`patch-134.md`).

## Hashes (identical in the German, English and French Steam installation and on the Deck; Russian below)

| File | Size (bytes) | SHA-256 |
|---|---|---|
| div.exe | 2 461 696 | `CD9F7A07C7A605C21052B3517346C7A63083063FB491276A3E4C9836C9B42A67` |
| configtool.exe | 280 576 | `053B19740948523FD5F785750BCD1771E8017A94A064F9AE203E5ED6AF339419` |
| DivDialogSystem.dll | 95 232 | `10C16C63B8881DD47160CD9DC876831EC9141C7A985C186AADF22499832C747C` |
| OsirisDLL.dll | 356 352 | `7D5F9E81F4E8C310C0FAF8FFA34E9872BFA2B07CA28E62C17E6E90412F6A0F95` |
| slash1.dll | 339 456 | `B2590BFE86956F142560558691BEBC489C4F3C0D761C3CEFCD8F75DEBD2EF3A5` |
| binkw32.dll | 357 939 | `65D87646770FA871A58D9D27CDFB5A1AD991DD64900F0EA35B0678457C902D41` |
| fmod.dll | 127 488 | `1DF3354B63EC6668A4831FCD02D12F36A6E4707435505F5C55DAD2F4AB62EEE9` |

`src/game/exe.cpp` (`kKnownBuilds`) knows this `div.exe` as "1.0062A (Steam)"; one set of signatures covers DE, EN and FR.

**Russian Steam version – a different build:** `div.exe` FileVersion `1, 0, 0, 61` (1.0061), linker timestamp 2012-08-01, 2 892 032 bytes, SHA-256 `90599AC1C2F9CC48BF83AEDB9850BA5B487B487E286D3B26C6E7967F0D928E68`, `.text` 2 564 547 bytes, same image base. Also different: `configtool.exe` (`D55DD8145C7270D714A6D9BF4B0F8190BCC7DBDDA5895D50A2B0719D7F2711E1`), `NlsGer.dll`; extra `NlsRus.dll`, `rttmText.dll`/`.dic`/`.tab` (probably what the `rttm on` key belongs to, `config-files.md`). `DivDialogSystem.dll`, `OsirisDLL.dll`, `slash1.dll`, `binkw32.dll` and `fmod.dll` are the same as in 1.0062A. The mod treats the EXE as unknown (no fixed-address fallbacks); signatures must be checked against it one by one.

## PE header of `div.exe`

- Linker 9.00 (VS 2008), timestamp 2013-04-19, runtime `MSVCR90.dll`.
- Image base `0x00400000`, relocations stripped, no ASLR → addresses are the same on every start (rule 8 still applies).
- One executable section `.text` (VA `0x1000`, 2 116 459 bytes mapped; file offset = VA − `0x400000` − `0xC00`); `.rdata` at VA `0x606000` (file `0x205000`), `.data` at VA `0x649000`.
- Large Address Aware **not** set → max. 2 GB address space without the 4GB patch.

## Language versions (Steam)

Working copies: `C:\dev\dd-test` (DE), `dd-test-en`, `dd-test-fr`, `dd-test-ru` – set up alike (dgVoodoo2, 2560×1440, `startup logo 0` except DE, Rivellon Renewed). DE, EN and FR share `div.exe`, DLLs, graphics, sounds, videos and `fonts/`; they differ only in:

| File | German | English | French |
|---|---|---|---|
| `config.lcl` | `language 2` | `language 1`, plus `debug on` (no effect, `config-files.md`) and `startup logo 0` (as shipped – the English copy plays no intro, the German one does) | `language 3`, plus `debug on`; intro on |
| `keylist.txt` | German comments | English comments (bindings identical) | same file as German |
| `dd_manual.pdf`, `Story.pdf` | German | English | French manual; no `Story.pdf` |
| `dat/<Language>/itemgen.cmp` | `dat/German/` | `dat/English/` | `dat/French/` |
| `localizations/<language>/` | `hints.txt`, `mono.cmp`, `skills.txt`, `text.cmp`, `voice.cmp` (171 MB) + dialog files **per player gender** in `male/`, `female/` and `dlg.v2/Male`, `dlg.v2/Female` | the same five files (`voice.cmp` 161 MB) + **one** set of dialog files (no gender split) | the same five files (`voice.cmp` 166 MB) + dialog files per gender in `Male/`, `Female/` (no `dlg.v2`) |

**Russian:** built on the German slot – `config.lcl` says `language 2` (no `debug on`), the texts are in `localizations/german/` and `dat/German/` (Russian content, code page 1251), dialogs per gender in `male/`, `female/` (no `dlg.v2`), `voice.cmp` 271 MB. Besides the binaries above, **all 61 `fonts/*.fnt` differ** (Cyrillic bitmap fonts, larger files), plus `dat/global.cmp`, three `static/back*.tga` (menu backgrounds, 26 bytes larger) and an HTML manual in `manual/`; no `Story.pdf`.

Language matters for the dialog loader (gender folders), text lengths in custom dialog boxes (Phase 6) and TTS voices (Phase 7).

**Rivellon Renewed per language (automated start, `FontHook=1`, windowed 2560×1440):**
- FR: EXE verified, all hooks and the font mode as in DE/EN, French main menu in Tahoma Bold, overlay normal. Accents are not on the main menu; they are part of the in-game check.
- RU: EXE known as "Russian edition 1.0061" (`kKnownBuilds`, best effort: no fixed-address fallbacks); input hook, overlay and GDI font hook work. The font mode signature (`FontInit`) does not match 1.0061, so the mod skips the font mode on this hash (log "Russian edition 1.0061 detected - font mode not supported, using original fonts") and the game draws its Cyrillic bitmap fonts correctly. With `use8bitfont 0` set by hand in `config.lcl`, the game creates the same 61 GDI fonts ("SimSun", weight 600, **`lfCharSet` 0 = ANSI_CHARSET**) and the Russian text comes out as Latin-1 garbage ("ÍÎÂÀß ÈÃÐÀ"): the bytes are code page 1251, GDI reads them as 1252. Replacing `lfCharSet` with `RUSSIAN_CHARSET` (204) in the font hook fixes it – main menu in correct Cyrillic Tahoma Bold; `[Fonts] Charset=auto` does this for the Russian hash (`fonts-mod.md`, `text-rendering.md` Consequences).

# Game version 1.0062A

Steam (and presumably GOG) ship `div.exe` FileVersion `1, 0, 0, 62` (= 1.0062A). It is based on the 1.0029A code base (the 1.0034A source was lost): widescreen and Windows 7 adjustments, but not all 1.0034A fixes. Savegames are not compatible with 1.0034A (`patch-134.md`).

## Hashes (identical in the German and English Steam installation and on the Deck)

| File | Size (bytes) | SHA-256 |
|---|---|---|
| div.exe | 2 461 696 | `CD9F7A07C7A605C21052B3517346C7A63083063FB491276A3E4C9836C9B42A67` |
| configtool.exe | 280 576 | `053B19740948523FD5F785750BCD1771E8017A94A064F9AE203E5ED6AF339419` |
| DivDialogSystem.dll | 95 232 | `10C16C63B8881DD47160CD9DC876831EC9141C7A985C186AADF22499832C747C` |
| OsirisDLL.dll | 356 352 | `7D5F9E81F4E8C310C0FAF8FFA34E9872BFA2B07CA28E62C17E6E90412F6A0F95` |
| slash1.dll | 339 456 | `B2590BFE86956F142560558691BEBC489C4F3C0D761C3CEFCD8F75DEBD2EF3A5` |
| binkw32.dll | 357 939 | `65D87646770FA871A58D9D27CDFB5A1AD991DD64900F0EA35B0678457C902D41` |
| fmod.dll | 127 488 | `1DF3354B63EC6668A4831FCD02D12F36A6E4707435505F5C55DAD2F4AB62EEE9` |

`src/game/exe.cpp` (`kKnownBuilds`) knows this `div.exe` as "1.0062A (Steam)"; one set of signatures covers both languages.

## PE header of `div.exe`

- Linker 9.00 (VS 2008), timestamp 2013-04-19, runtime `MSVCR90.dll`.
- Image base `0x00400000`, relocations stripped, no ASLR → addresses are the same on every start (rule 8 still applies).
- One executable section `.text` (VA `0x1000`, 2 116 459 bytes mapped; file offset = VA − `0x400000` − `0xC00`); `.rdata` at VA `0x606000` (file `0x205000`), `.data` at VA `0x649000`.
- Large Address Aware **not** set → max. 2 GB address space without the 4GB patch.

## German vs. English installation

Everything is identical (364 files: EXE, DLLs, graphics, sounds, videos, fonts) except:

| File | German | English |
|---|---|---|
| `config.lcl` | `language 2` | `language 1`, plus `debug on` (no effect, `config-files.md`) and `startup logo 0` |
| `keylist.txt` | German comments | English comments (bindings identical) |
| `dd_manual.pdf`, `Story.pdf` | German | English |
| `dat/<Language>/itemgen.cmp` | `dat/German/` | `dat/English/` |
| `localizations/<language>/` | `hints.txt`, `mono.cmp`, `skills.txt`, `text.cmp`, `voice.cmp` (171 MB) + dialog files **per player gender** in `male/`, `female/` and `dlg.v2/Male`, `dlg.v2/Female` | the same five files (`voice.cmp` 161 MB) + **one** set of dialog files (no gender split) |

Language matters for the dialog loader (gender folders), text lengths in custom dialog boxes (Phase 6) and TTS voices (Phase 7).

# RE-Notizen

Erkenntnisse in eigenen Worten. **Kein dekompilierter Code, kein Pseudocode** (siehe CLAUDE.md, Regel 2).

## Spielversion (Steam)

- Version laut Ressourcen von `div.exe`: FileVersion `1, 0, 0, 62` (= 1.0062A)
- Installationsordner: 363 Dateien, ca. 2,4 GB

| Datei | Größe (Bytes) | SHA-256 |
|---|---|---|
| div.exe | 2 461 696 | `CD9F7A07C7A605C21052B3517346C7A63083063FB491276A3E4C9836C9B42A67` |
| configtool.exe | 280 576 | `053B19740948523FD5F785750BCD1771E8017A94A064F9AE203E5ED6AF339419` |
| DivDialogSystem.dll | 95 232 | `10C16C63B8881DD47160CD9DC876831EC9141C7A985C186AADF22499832C747C` |
| OsirisDLL.dll | 356 352 | `7D5F9E81F4E8C310C0FAF8FFA34E9872BFA2B07CA28E62C17E6E90412F6A0F95` |
| slash1.dll | 339 456 | `B2590BFE86956F142560558691BEBC489C4F3C0D761C3CEFCD8F75DEBD2EF3A5` |
| binkw32.dll | 357 939 | `65D87646770FA871A58D9D27CDFB5A1AD991DD64900F0EA35B0678457C902D41` |
| fmod.dll | 127 488 | `1DF3354B63EC6668A4831FCD02D12F36A6E4707435505F5C55DAD2F4AB62EEE9` |

Beobachtungen aus der Dateiliste (noch ungeprüft):
- Audio vermutlich über **FMOD** (`fmod.dll`), Videos über **Bink** (`binkw32.dll`).
- `slash1.dll` … `slash4.dll` plus `slashed-d3d6.cfg`, `slashed-directx.cfg`, `slashed-software.cfg` – vermutlich die Renderer-Module.
- `SECDRV.SYS`, `DrvMgt.dll` – Reste des Kopierschutzes.
- Sprach-DLLs: `NlsEng.dll`, `NlsFr.dll`, `NlsGer.dll`.

## Imports

_Offen (Phase 0, Schritt 3)._

## Entscheidungen

- Proxy-DLL: _offen_
- Overlay: _offen_

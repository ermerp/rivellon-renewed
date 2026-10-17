# RE-Notizen

Erkenntnisse in eigenen Worten. **Kein dekompilierter Code, kein Pseudocode** (siehe CLAUDE.md, Regel 2).

## 1. Spielversion (Steam, deutsche Sprachfassung)

- Version laut Ressourcen von `div.exe`: FileVersion `1, 0, 0, 62` (= 1.0062A)
- Installationsordner: 363 Dateien, ca. 2,4 GB; Sprache laut `config.lcl`: `language 2` (Deutsch)

| Datei | Größe (Bytes) | SHA-256 |
|---|---|---|
| div.exe | 2 461 696 | `CD9F7A07C7A605C21052B3517346C7A63083063FB491276A3E4C9836C9B42A67` |
| configtool.exe | 280 576 | `053B19740948523FD5F785750BCD1771E8017A94A064F9AE203E5ED6AF339419` |
| DivDialogSystem.dll | 95 232 | `10C16C63B8881DD47160CD9DC876831EC9141C7A985C186AADF22499832C747C` |
| OsirisDLL.dll | 356 352 | `7D5F9E81F4E8C310C0FAF8FFA34E9872BFA2B07CA28E62C17E6E90412F6A0F95` |
| slash1.dll | 339 456 | `B2590BFE86956F142560558691BEBC489C4F3C0D761C3CEFCD8F75DEBD2EF3A5` |
| binkw32.dll | 357 939 | `65D87646770FA871A58D9D27CDFB5A1AD991DD64900F0EA35B0678457C902D41` |
| fmod.dll | 127 488 | `1DF3354B63EC6668A4831FCD02D12F36A6E4707435505F5C55DAD2F4AB62EEE9` |

### PE-Header `div.exe` (1.0062A)

- Linker 9.00 (VS 2008), Zeitstempel 19.04.2013, Laufzeit `MSVCR90.dll`
- Image-Base `0x00400000`, **Relocations entfernt, kein ASLR** (DLL characteristics `0x8000` = nur Terminal-Server-aware) → Adressen innerhalb dieses Builds sind bei jedem Start gleich. Regel 8 gilt trotzdem (Signatur-Scan, feste Adressen nur als Fallback).
- **Large-Address-Aware nicht gesetzt** (Characteristics `0x103`) → ohne 4GB-Patch max. 2 GB Adressraum.

## 2. Imports und Laufzeit-Architektur (1.0062A)

Statische Abhängigkeiten von `div.exe` (Anzahl importierter Funktionen):

| DLL | # | Zweck / wichtigste Funktionen |
|---|---|---|
| fmod.dll | 32 | Audio über **FMOD 3.x** (`FSOUND_*`: Samples, Streams, 3D-Sound, Lautstärke) |
| binkw32.dll | 9 | Videos über **Bink** (`BinkOpen`, `BinkDoFrame`, `BinkCopyToBuffer`, `BinkOpenDirectSound`) |
| DivDialogSystem.dll | 12 | Dialog-Engine als C++-Klasse `CDivDialogSystem` |
| OsirisDLL.dll | 14 | Osiris-Skript-VM als C++-Klasse `COsiris` (globale Instanz exportiert) |
| USER32.dll | 28 | Fenster/Nachrichten, `GetAsyncKeyState`, `GetKeyState`, `MapVirtualKeyA`, `GetCursorPos` |
| GDI32.dll | 8 | Schriften (`CreateFontA`, `GetGlyphOutlineA`) – vermutlich Text-Rendering für Unicode-Sprachen |
| IMM32.dll | 3 | IME-Texteingabe |
| KERNEL32.dll | 54 | u. a. `LoadLibraryA`, `GetProcAddress`, `SetThreadAffinityMask` |
| VERSION.dll | 3 | `GetFileVersionInfo*` |
| WINMM.dll | 1 | nur `timeGetTime` |
| MSVCR90.dll | 170 | C-Laufzeit |

Ergebnisse zu den Fragen aus PLAN.md Phase 0, Schritt 3:

- **Grafik:** `div.exe` importiert **kein** DirectDraw/Direct3D. Die Renderer sind Plugins, die zur Laufzeit per `LoadLibraryA`/`GetProcAddress` geladen werden. Alle vier exportieren dieselbe Schnittstelle (`DllSlashedInit`, `DllSlashedStartFrame`, `DllSlashedEndFrame`, `DllSlashedGetResolutions`, `DllSlashedGlowDraw*`, `DllSlashedShutdown`, …):

  | DLL | Renderer | Konfig | Grafik-API |
  |---|---|---|---|
  | slash1.dll | Direct3D 6.x | `slashed-d3d6.cfg` | `ddraw.dll` (DirectDraw + Direct3D 6) |
  | slash2.dll | Glide 3.x | `slashed-glide.cfg` | `glide3x.dll` (nicht vorhanden) |
  | slash3.dll | DirectDraw | `slashed-directx.cfg` | `ddraw.dll` |
  | slash4.dll | Software | `slashed-software.cfg` | `ddraw.dll` (nur Ausgabe) |

  Auch der Direct3D-Renderer läuft also über `ddraw.dll` (D3D6 wird über das DirectDraw-Objekt erzeugt; `D3DImm.dll` lädt `ddraw.dll` intern). dgVoodoo2 (`ddraw.dll` + `D3DImm.dll`) deckt damit Direct3D, DirectDraw und Software ab.
  **Folge für die Mod:** `DllSlashedStartFrame`/`DllSlashedEndFrame` sind ein renderer-unabhängiger Frame-Takt – guter Hook-Punkt für Logik pro Frame (z. B. durch Abfangen von `GetProcAddress` für diese Namen).
- **Eingabe:** **Kein DirectInput.** Tastatur/Maus über Fenster-Nachrichten (`PeekMessageA`/`GetMessageA`) sowie `GetAsyncKeyState`/`GetKeyState`/`GetCursorPos`. → Input-Hook über Fensterprozedur (Subclassing) und Hooks auf diese USER32-Funktionen.
- **Audio:** FMOD 3.x (`fmod.dll`, nutzt intern DirectSound/WINMM). Kein Miles. Für Ducking (Phase 7) bieten sich `FSOUND_SetVolume`/Stream-Funktionen an.
- **Video:** Bink (`binkw32.dll`), Ton über `BinkOpenDirectSound`.
- **Dialoge:** `DivDialogSystem.dll` exportiert u. a. `StartDialog`, `GetNumQuestions`, `GetQuestion` (liefert Wide-String), `GetAnswerText`, `GetAnswerSoundName`, `SelectQuestion`, `GetQuestionNodeID`, `GetAnswerNodeID`. → Für Phase 6 (eigene Dialogbox) lassen sich Text und Auswahl direkt an diesen Exporten abgreifen, ohne die EXE zu analysieren.
- **Osiris:** `OsirisDLL.dll` exportiert `COsiris::Event`, `Load`, `Save`, `InitGame`, `GetStoryVersion`, `ApplyPatchFile` u. a. – Basis von OsirisRE.

Weitere DLLs im Ordner: `configtool.exe` importiert `DDRAW.dll` (Auflösungsliste); `NlsEng/NlsFr/NlsGer.dll` brauchen `mfc90.dll`; `DrvMgt.dll`/`SECDRV.SYS` sind Kopierschutz-Reste und werden von `div.exe` nicht importiert.

## 3. Entscheidungen

### Proxy-DLL (Phase 0, Schritt 4) – Vorschlag

`div.exe` importiert statisch nur zwei Systembibliotheken, die sich als Proxy eignen: **`winmm.dll`** (1 Funktion) und **`version.dll`** (3 Funktionen). `dinput.dll` kommt **nicht in Frage** (wird nie geladen). `ddraw.dll`/`D3DImm.dll` sind für dgVoodoo2 reserviert.
→ Bestätigt den Favoriten: **Ultimate ASI Loader als `winmm.dll`**, Mod als `.asi`. `version.dll` als Ausweichlösung. Beide Namen importiert auch die 1.0034A-EXE. Hinweis: `fmod.dll` und `binkw32.dll` importieren ebenfalls `WINMM.dll` – sie erhalten dann dieselbe Proxy-Instanz, daher muss der Loader alle WINMM-Exporte vollständig weiterleiten (beim Ultimate ASI Loader der Fall).

### Overlay (Phase 0, Schritt 5) – offen, mit dem Menschen abzustimmen

_Siehe Bewertung im Gesprächsverlauf; Entscheidung wird hier eingetragen._

## 4. Patch 1.34A (englisch) – Inhaltsanalyse

Quelle: `C:\dev\downloads\DivPatch_English.exe` (10 883 072 Bytes, SHA-256 `132BE8A2ED741FB055449FB4983D0CCD5016C0D967ACCE6B0A21088D979B3D01`, nicht signiert). Selbstentpackendes RAR in einer PE-Ressource; mit 7-Zip (`7z x -trar`) entpackt nach `C:\dev\downloads\patch134_en\files`, **nicht ausgeführt**. `patch.log`: „Divine Divinity Patch V1.0 - 13-03-03 - English.“

Inhalt (80 Dateien) und Vergleich mit der deutschen Steam-Installation:

| Datei(en) | Steam (DE, 1.0062A) | Sprachabhängig? |
|---|---|---|
| `div.exe` (1.0.0.34, 02.05.2003) | anders | nein – EXE ist mehrsprachig (enthält deutsche CD-Meldungen, Sprachtabellen DE/EN/FR) |
| `Osiris.dll` | fehlt (Steam heißt `OsirisDLL.dll`) | nein |
| `DivDialogSystem.dll` (1.5.0.1) | anders | nein – aber **andere Schnittstelle** (Strings als `unsigned short*` statt `wchar_t*` im Namen) → passt nur zur 1.0034A-EXE |
| `slash1–4.dll` | anders (ältere Builds) | nein |
| `DoSomething.exe` („DivHeroPatch MFC Application“) | fehlt | Hilfsprogramm des Installers, nicht nötig |
| `Dat\global.cmp`, `Dat\treasure.cmp` | anders | nein (Wortstatistik identisch zur Steam-Datei) |
| übrige `Dat\*` | identisch | – |
| `Global\region.000–007` | anders (gleiche Größe) | nein |
| `Global\eggs.000`, `location.*` | identisch | – |
| `Main\startup.v2\*` (60 Dateien) | fehlt; gegenüber `main\startup` sind 21 Dateien geändert (u. a. `story.000/001`, `items.000`, `data.000`, `world.x0`, `shroud.x*`, `inv.*`), Rest identisch | nein – keine Sprachtexte gefunden; `static\books.000` und `quest_log.000` sind identisch mit Steam-DE |
| `Localizations\English\dlg.v2\dialogids.dat` | identisch mit DE | nein |
| `Localizations\English\dlg.v2\dialogs.div` | **identisch mit Steam `german\dlg.v2\Male\dialogs.div`** | nein (Dialogstruktur) |
| `Localizations\English\dlg.v2\dialogtxt.dat` | anders | **ja – englischer Dialogtext** |

Wichtige Beobachtungen:
- Die deutsche Steam-Version enthält **bereits** `localizations\german\dlg.v2\Male|Female` mit der 1.33-Dialogstruktur. Es fehlt nur `main\startup.v2`.
- Die 1.0062A-EXE kennt weder `dlg.v%d` noch `startup.v%d` (Strings fehlen) – sie nutzt `dlg.v2` und `startup.v2` also nicht. Die 1.0034A-EXE sucht genau `localizations\%s\dlg.v%d` und `main\startup.v%d`.
- Einzige sprachabhängige Datei im Patch: `dialogtxt.dat` (englisch). Für eine deutsche Installation wird sie nicht gebraucht, weil das deutsche Gegenstück schon vorhanden ist.
- Die 1.0034A-EXE enthält CD-Abfragetexte („Bitte legen Sie die CD namens %s …“, `cdrom path`) und importiert `unrar.dll` (in Steam vorhanden), `MSVCRT.dll`, `Osiris.dll`.

### PE-Header `div.exe` (1.0034A)

SHA-256 `E5664343DA564BF015CD0136E50FD1A79FC1AF1B84EAFE4D9A8760C17E427639`, Linker 6.00 (VC6), Image-Base `0x00400000`, kein ASLR, kein Large-Address-Aware.

## 5. Versionslage

- Steam/GOG 1.0062A basiert auf dem Code-Stand 1.0029A (Quellcode von 1.0034A verloren); hat Widescreen und Windows-7-Anpassungen, aber nicht alle Fixes aus 1.0034A. Spielstände zwischen den Versionen nicht kompatibel.
- Laut Larian (Raze, Steam-Forum) unterstützt 1.0034A offiziell keine Auflösungen über 1024×768; Community-Berichte sagen, dass es mit dgVoodoo trotzdem läuft → muss getestet werden.
- Laut RPG Codex (2003) erschien 1.34 auch für die deutsche und französische Fassung (deutsch über CDV). Eine offizielle Bezugsquelle des deutschen Patches ist noch nicht bestätigt.

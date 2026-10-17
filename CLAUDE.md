# CLAUDE.md – dd-modern

Modernisierungs-Mod für Divine Divinity (Steam/GOG 1.0062A) als Proxy-DLL. Vollständiger Plan: [PLAN.md](PLAN.md). Phasen der Reihe nach abarbeiten.

## Pfade

- Original-Spiel (Steam): `C:\Program Files (x86)\Steam\steamapps\common\divine_divinity` – **nur lesen, nie verändern.**
- Test-Arbeitskopie: `C:\dev\dd-test` – hier werden DLLs, dgVoodoo2, INI usw. installiert und getestet.
- Repo: `C:\dev\dd-modern`
- Ghidra 12.1.4: `C:\tools\ghidra_12.1.4_PUBLIC` (JDK: Temurin 21)
- x32dbg: `x32dbg` (winget-Alias)
- MSVC: `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools`

## Feste Regeln

1. **Keine Larian-Dateien ins Repo** (EXE, DLLs, Grafiken, Texte, Audio, Schriften). `.gitignore` pflegen.
2. **Kein dekompilierter Code / Pseudocode ins Repo** – auch nicht KI-umformuliert. Erkenntnisse in eigenen Worten in `docs/re-notes.md` (Zweck, Signatur, Adresse, Struktur-Layout). Implementierung immer eigener Code.
3. **Keine Binär-Patches der EXE.** Alles über die Proxy-DLL.
4. **Jedes Feature einzeln abschaltbar** über `ddmod.ini`. Eingreifende Features (Schnellreise, Balancing) standardmäßig aus.
5. **Abgeleitete Inhalte nur lokal beim Nutzer erzeugen** (Upscaling, TTS). Die Mod liefert Tools, keine Daten.
6. **Keine geklonten Stimmen echter Sprecher** – nur generische synthetische Stimmen.
7. **Spielstände schützen:** vor Tests mit Schreibzugriff auf Spielzustand Saves sichern; zustandsverändernde Features erst mit Wegwerf-Saves testen.
8. **Keine hart codierten Adressen** – Signatur-Scan; feste Adressen nur als dokumentierter Fallback für die per SHA-256 geprüfte EXE.
9. **Nachfragen statt raten:** Für Ingame-Tests klare Testanweisung (was tun, was beobachten) in `docs/testing.md` und auf Ergebnis warten.

## Technik

- Spiel ist **32-bit** → immer x86 bauen (MSVC `-A Win32` bzw. x86-Toolchain).
- Build: CMake (+ Ninja), MSVC Build Tools 2022.
- Nur die Spiel-EXE/-DLLs analysieren (Inhalte gehen an den Modell-Anbieter).

## Arbeitsweise pro Feature

Untersuchen → in `docs/re-notes.md` dokumentieren → minimal hinter Feature-Flag implementieren → Testanweisung in `docs/testing.md` → ausbauen → Commit. Ein Feature pro Branch.

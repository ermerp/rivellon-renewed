# Key events and `keylist.txt`

## Events

The EXE registers **40 key events** (`0x489D00`–`0x48A0D0`, source file `divevent.cpp`). **`keylist.txt` ID = −(index + 1).**

| ID | Event | Name source |
|---|---|---|
| −1 … −20 | Drop Console, Toggle KeyPlate, Icon Hotkey Assign, Toggle OptionsPlate, Toggle NonEnergy Plate Visibility, Toggle Plate Visibility, Toggle Show Icon Hotkey, Open Load Plate, Open Save Plate, Open Status Plate, Open Inventory, **StartEditor (−12)**, **SwitchEditor (−13)**, PauseGame, **NextHour (−15)**, AutoMap, **ClearShroud (−17)**, ScaleAutoMapUp, ScaleAutoMapDown, SwitchCombatMode | localized (text ID `0x2BC + index`); English names also in `divinityevent.dat` |
| −21 | Play Test Sound | hard-coded (debug) |
| −22 … −25 | Show Equipment, Open Automap, Open Questlog, Take Screenshot | localized |
| −26 … −29 | Cheat One Level Up, Create Bugreport, Show Buglist, Teleport | hard-coded (debug) |
| −30 … −32 | Always Run, Quick Load, Quick Save | localized |
| −33 … −40 | DBG Update Shroud, Start a monologue, Start/Stop memory logging, Clear memory log, Dump memory log, Update all skills, Clear all skills | hard-coded (debug) |

Verified against the generated `keylist.txt` (−8, −16, −20, −30 … −32). Whether debug events run without the developer flag (`developer-mode.md`) is still open: they cannot be bound – `keylist.txt` is not read back and the in-game key configuration lists only the regular events (T2.0-W). Follow-up in Phase 2 (`open-questions.md`).

## `divinityevent.dat`

388 bytes, plain text, identical in DE/EN: the 20 names of events 0–19, one per line in quotes. (The forum calls it "divineevent.dat".)

**Zoom events −18/−19:** DE "Hineinzoomen"/"Herauszoomen", EN "Scale Automap up"/"Scale Automap down" – the automap's scale, no world zoom. No default key. The tester bound them in the key configuration (DE `keylist.txt` then shows `-18⇥-⇥-⇥o` and `-19⇥-⇥-⇥p`: the key lands in a field after the usual `-`, unlike every other binding); neither the world nor the map (M) changed. Open: whether the binding format is the reason, whether they act only on the minimap/automap window, or whether they are dead (`open-questions.md`); automap code with zoom buttons at `0x449FF0`–`0x44BFDB` (`engine-map.md`).

## `keylist.txt` format

Written by `0x48A330` on exit ("File automatically generated"); only events with a binding are written. **Export only – the game does not read it back:** five bindings added by hand (F5–F9, T2.0-W) had no effect and were gone after the next exit (file rewritten byte-identical to before). The EXE also has the strings "Export Keylist" and "Generate keylist". Where the bindings are really stored is open (`persist.dat` is rewritten on exit too – candidate; `open-questions.md`).

- Optional comment line `# <event name>` (`  (fixed)` appended for fixed keys).
- Binding (⇥ = one Tab): `<id>⇥<modifier> <key>⇥-` – modifier `-` (none), `shift`, `alt` or `ctrl`; key = a character or a key name: `f1`–`f12`, `num_0`–`num_9`, `num_sub`, `num_mul`, `num_div`, `num_del`, `space`, `enter`, `escape`, `backspace`, `insert`, `delete`, `home`, `pgup`, `pgdn`, `uparrow`, `downarrow`, `leftarrow`, `rightarrow`, `pause`, `scrolllock`, `printscreen`, `clear`, `cancel`, `help`.
- Fixed key: `<id>⇥-⇥-⇥<char>`.

Related: `console.lst` is read at startup (`0x40D8CF`); `debug.lst`, `mdebug.lst`, `debugdll.lst` exist only as strings.

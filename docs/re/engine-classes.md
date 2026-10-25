# Classes and DLLs of the engine

Class hierarchies recovered from RTTI and the game's own DLLs – part of the engine map (`engine-map.md`). Addresses are for the verified 1.0062A build (`game-version.md`).

## Class hierarchies (RTTI)

586 classes with a virtual function table (read from the EXE's RTTI: complete object locators, class hierarchy descriptors, vftables). 57 root classes, inheritance up to 9 levels deep, multiple inheritance only twice (`std::basic_iostream`, the editor's `ObjLstWindow`). Largest hierarchies (descendants, virtual methods of the root):

| Root | Descendants | Virtual methods | Content |
|---|---:|---:|---|
| `CSkill` | 96 | 7 | every skill and spell skill (`CMagicWizard*Skill_*`, `CSurvivorLoreSkill_*`, …) |
| `CMagicEffect` | 60 | 12 | running spell and status effects (`CFireBallSpell`, `CIceMagic`, `CInterpreterSpell` = scripted spells) |
| `CBitmapControl` | 41 | 23 | **UI controls**: buttons, lists, scroll bars, sliders, tabs, text views, energy bars, mini map |
| `CSubWindow` | 39 | 15 | **UI windows ("plates")**: control, book, toggle, charm, monologue, dialog, trade, inventory, … |
| `CProjectile` / `CArrow` | 37 / 27 | 6 / 1 | projectile types and their visuals |
| `CAgentTactics` | 19 | 2 | combat tactics, including boss-specific "trickster" tactics |
| `EditorModule` | 19 | 6 | editor modes |
| `CAgentBehavior` | 17 | 4 | NPC behaviors (citizen, guard, tavern guest, market buyer, …) |
| `CExpression` / `CStatement` | 13 / 6 | 3 / 3 | syntax tree of the script interpreter |
| `CPath` | 12 | 7 | movement paths (linear, spline, orbit, helix, attach to NPC) |
| `CDIVINITYOsirisFunction` | 10 | 2 | functions the game offers to Osiris, by target (NPC, object, region, location, dialog) |
| others | | | `CGroupBehavior` 7, `CItemGenerationDescription` 7, `CExplosion` 5, `CPainPoint` 4, `InterfacePlate` 3 (map, notes, diary), `CAbstractInventoryModel` 3 (NPC, object, offered goods), `CBMTextViewTOracle` 5 (text sources for dialog, answers, talk log, monologue) |

Most virtual methods: `CPartyMember` 34, `CNpc` 32, `CAgent` 30, `CBMTextButton` 28, the scroll/slider/check-box controls 25. The vftable of each class lists its virtual methods in a fixed order; matching the slots across a hierarchy gives the meaning of each slot (e.g. draw, handle input, update) – the interface an own class must provide to stand in for an original one.

**For replacing subsystems:** the UI is an object hierarchy with two base classes (`CSubWindow` 15 slots, `CBitmapControl` 23 slots). An own window or control can be put in by implementing those slots – the game would call it like its own – once the slot meanings are documented. Skills, spell effects, projectiles, tactics and behaviors work the same way, which makes adding or changing one of them a contained task.

## Larian DLLs

| DLL | Functions | `.text` bytes | Notes |
|---|---:|---:|---|
| `OsirisDLL.dll` | 1 726 | 266 752 | Osiris story/script engine; 49 exports, 57 RTTI classes, CRT linked statically (only KERNEL32 imported) |
| `DivDialogSystem.dll` | 483 | 67 584 | dialog system; 16 exports |
| `slash1.dll` (Direct3D) | 1 160 | 269 312 | 15 exports each (`renderer-d3d6.md`) |
| `slash2.dll` (Glide) | 1 183 | 250 880 | imports `glide3x.dll` |
| `slash3.dll` (DirectDraw) | 1 190 | 292 352 | |
| `slash4.dll` (software) | 1 190 | 282 112 | |
| `NlsEng/Fr/Ger.dll` | – | – | language resources, 16 KB each |

About 1 030 functions (184 KB) of each `slash*.dll` have identical sizes in all four – static CRT plus a shared renderer base; the renderer-specific part is about 55–90 KB per DLL. `DrvMgt.dll` (2002, exports `Setup`/`Remove`, 40 KB) is named neither by `div.exe` nor by `configtool.exe` – probably an installer leftover, not analyzed. `binkw32.dll`, `fmod.dll` and `unrar.dll` are third-party (rule 10).

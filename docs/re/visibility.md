# Visibility of persons (hero's sight range)

Why persons vanish on large screens, and where the game decides it. Addresses for 1.0062A (`game-version.md`); code must find them by signature (rule 8).

## Observation

At 1920×1080 an NPC in front of a house door disappeared when the view moved and she was ≈450–500 px left of the screen center, while the ground under her was still drawn (human's play session). The cut-off is **not** a screen-size constant: the world and sprite code use the real resolution (globals `0x64D9B8` width / `0x64D9BC` height, 640×480 only as static default). It is the hero's **sight range**, a game value.

## How it works

- **Agent table:** the global `0x658D50` points to the agent manager; `+0x0` is a list of agent ids, `+0x4` its count (the "Visual agents" number of the debug info line, `0x4AB750`), `+0xC` the id → agent object table, `+0x14` a count used as the upper bound for story events.
- **Position of an agent:** x = `+0x04` + `+0x1C`, y = `+0x08` + `+0x20` (world units; a tile is 32 units, used by `>> 5` in several places). Level/map id at `+0x284`; two agents on different maps never see each other.
- **Distance `0x40ECB0`:** an integer approximation of the straight-line distance (larger axis plus fixed fractions of both axes, no square root) between two points.
- **Sight range:** the observer's stats object (agent `+0x2C`) field `+0x20`, multiplied by 32 → the range in world units, so one sight point is one tile. Its current value is a sum (below); the hero's usual size has not been measured yet.
- **How the current sight is built** (stats code around `0x55A2A0`–`0x55BC80`): the stats object keeps base values (`+0x48`…`+0x68`) and current values (`+0x14`…`+0x34`); a recompute copies the base sight `+0x54` into the current `+0x20`, then adds the active modifiers (a list of 20-byte entries, type 4 = sight, `0x55A032`/`0x55A0F2`) and the bonuses of the eleven equipment slots (`0x55BC41`); sight never drops below 1.
  - The skill **Ranger sight** (EN "Elven Sight", DE "Elfenblick"; class `CSurvivorTalentSkill_Rangersight`) adds a value from the table `RangersightBoost` to the **base** `+0x54` when learned or raised (`0x544B80`, `0x544CA0`; index = skill value / 20), so it reaches the current sight through the recompute.
  - Items and potions with "+X to sight" (text ids 38/39, 434/435) feed the modifier and slot sums.
  - Scripts set an NPC's sight directly: the script command `set sight #` and the agent's sight states (agent `+0x2B4` table, index `+0x2BA`) write `+0x20` (`0x434E19`; debug line "Npc %s changed sight to %d" with the developer flag).
  - Related skill parameters found by name, writers not traced: `TrueSightRange` (True Sight), `BlindSightLost` (Blind), `DivineEyeDuration`, `WizardEyeDuration` (Wizard's Sight), spell class `CEagleEyeSpell` ("Eagle Eye", `eagleeye.mgc`).
- **Hard to see targets:** if the target has flag `0x20` in its agent flags (`+0x220`), the range drops to a half by day and a quarter at night (hour < 5 or > 22; hour from `0x50BFE0` = game clock mod 24).
- **Line of sight `0x56FBC0`:** after the distance test the game checks that nothing blocks the straight line between both points. Targets with flags `0x10000100` in `+0x224` (dead/hidden) are never visible.

Three functions apply this rule:

| Function | Observer → target | Used for |
|---|---|---|
| `0x435570` | **hero → agent** | presentation only: the fade of an agent's sprites (`0x4496F0`, alpha `+0x274` steps of 10 between 0 and 100, sprite flags via `0x547B40`) and whether a click on an agent counts (`0x4A5760`) |
| `0x435410` | agent → agent | AI: whether one NPC sees another (nine callers in `agents`) |
| `0x4356F0` | agent → all agents | AI: an agent's list of up to 16 seen agents, with "now sees"/"lost sight" story events (`0x50C880`/`0x50CF60`); called from the agent manager update |

The fade function (`0x4496F0`) re-runs the test only when the hero's or the agent's tile changes; outside the range the agent fades out over ten frames, inside it fades in.

**Global switch:** settings object (`0x65B3D0`, `config-files.md`) field `+0x2C`. The constructor sets it to 1, and no config key writes it (`line of sight on/off` is parsed but ignored in 1.0062A). With 0 the fade and click code skips the test – every agent on screen stays visible.

## What it means for the mod

- The range was evidently made for the original screens: the vanishing point seen at 1920×1080 (≈450 px from the center) is close to half the width of an 800×600 or 1024×768 screen; the hero's actual sight value is not measured yet. At 1920×1080 or 2560×1440 much of the visible world lies outside the range, so persons there are not drawn.
- **Fix that keeps the game rule:** hook `0x435570` (only two callers, both presentation) and scale the range by the screen size relative to 800×600 (or a factor from the INI) before the distance test – AI perception (`0x435410`, `0x4356F0`) stays untouched, so fights and story events do not change. The line-of-sight check stays.
- **Sight bonuses are part of the hero's range:** Elven Sight and "+sight" items raise the same `+0x20` the presentation test reads, so any change to the range changes what they are worth. And the same test decides whether a **click on an agent counts** (`0x4A5760`) – a larger range lets the hero target enemies farther away, a gameplay effect (ranged combat), not only a visual one. Variants: (a) multiply the whole range by the screen factor – the bonus keeps its share, its absolute gain grows; (b) add the difference between the screen's half size and 800×600's to the range – every bonus keeps its absolute size in tiles; (c) only raise the range to the screen edge (`max`) – bonuses stop mattering until they exceed the screen; (d) scale only for the fade, not for clicks – persons stay visible but cannot be clicked beyond the original range.
- **Simplest switch:** settings `+0x2C` = 0 shows every agent on screen, through walls too – a bigger change to the game's feel; at most an option.
- Whether the shroud (black unexplored area) uses the same range is not checked.

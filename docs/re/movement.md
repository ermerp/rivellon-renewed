# Hero movement by mouse (walk/run)

How mouse input makes the hero walk or run. Addresses for 1.0062A (`game-version.md`); code must find them by signature (rule 8). Found statically; confirmed at runtime (human in game, T1.19-W with log hooks on the run setter and the always-run getter).

## Running in the agent

- **Run state:** agent flag `0x08000000` in `+0x220`; switching it on doubles the speed byte `+0x217`, switching it off halves it again.
- **Two setters** (virtual methods of the hero class `CPartyMember`, vtable `0x6098CC`):
  - `+0x40` → `0x4283B0` (shared by all agents): run on/off, second argument sets an extra flag (`+0x224` bit `0x8`).
  - `+0x84` → `0x441030` (party members only): the same, but only while the hero is not being pushed (`+0x22C`/`+0x230` both zero) or `+0x278` < 1.
- **Running is refused or ended** when agent flag `0x40000000` in `+0x220` is set or `+0x354` is 0 – both setters check it, and the hero's update (`0x441770`, vtable `+0x1C`) switches running off every frame while it holds. What the two fields mean (overload, a cut-scene, a state, a stat?) is not checked.

## Player input

- **Player input per frame `0x4A8A50`** (`GAME\Player.cpp`, player object = `this`, the global `0x658C04`): reads the mouse state object (`[this]`, cursor x/y at `+0x20`/`+0x24`, left button held at `+0x68`) and hands it to sub-handlers.
- **Holding the left button on the ground → `0x4A6360`** (every frame while held): takes the cursor in screen pixels (player `+0x51C`/`+0x520`) and its offset from the **screen center**. Outside a circle with radius **3/8 of the screen height** (integer division; squared values compared, no square root) or with player `+0x480` ≠ 0 → setter `+0x84` with 1 (run), mouse object `+0x78` = 5; inside → setter with 0 (walk), `+0x78` = 4. Then the hero heads straight for the cursor (`0x441250`; world point = view position at player `+0x4A0`/`+0x4A4` plus cursor). Moving the cursor across the circle switches between walking and running during the walk.
- **Releasing the button → `0x4A8320`:** with mouse object `+0x60` = 0 every release counts as a **click**; otherwise only a press under 0.5 s, or under 1 s with the cursor moved less than 16 px (meaning of `+0x60` not checked; in the test copies every release counted, also after holding 1.5 s; yet the hero stops on release in game (tester), so that click move does not carry him on – why is open) – the click and calls `0x4A81D0`: it finds a free spot near the cursor and sends the hero there **with pathfinding** (`0x5173E0`, for every selected party member). That move sets running through setter `+0x40` with the value of player `+0x480` (getter `0x4A6FA0`, called only from there) – **a click walks unless `+0x480` is set**, no matter how far away.
- **Clicking an item → `0x4A79F0`:** pathfinding move to the item, running also only with `+0x480`.
- **Player `+0x480`:** the only readers are the three above; the only writer found is the player constructor (`0x4A90E0`, sets 0). It is presumably the "Always Run" toggle (key event, `key-events.md`), but its writer at runtime is not found – it may be written through another path.

## Code shape at the circle check (for the signature)

From `0x4A65AE`: load width `0x64D9B8` and height `0x64D9BC`, load cursor x/y from `+0x51C`/`+0x520`, halve both screen sizes (signed `cdq / sub / sar 1`), subtract them from the cursor, then `lea eax,[ebx+ebx*2]` (3 × height), signed division by 8 (`cdq / and edx,7 / add / sar 3`), square all three values, `cmp` / `jg` to the run branch, then `cmp dword [esi+0x480],0` and the two `call [edx+0x84]` with `push 0` / `push 1`. Unique byte run for a scan: from `lea eax,[ebx+ebx*2]` through `cmp dword [esi+0x480],0`.

## Why the walk zone feels too large at high resolutions

The radius follows the screen height, but the game draws the world 1:1 (more map instead of larger sprites, T1.6-W C), so in world terms the walk zone grows with the resolution:

| Resolution | Radius (px = world pixels) | Run band above/below the circle |
|---|---|---|
| 800×600 | 225 | 75 px |
| 1024×768 | 288 | 96 px |
| 1920×1080 | 405 | 135 px |
| 2560×1440 | 540 | 180 px |

## When the hero is not at the screen center

The camera follows the hero every frame (`0x4A4BC0`: view = hero position minus half the screen), but:
- **Level edges:** the view setter `0x4A3D20` clamps the view to the current level's size (`0x75182C` `+0x4`/`+0x8`, in units of 64 px) and to the world limits (`0x7F80`/`0xFF80` minus the screen size). Near the edge of a small level the camera stops and the hero walks off center.
- **Shift + cursor at the screen edge** scrolls the view away from the hero (`0x4A4260`); without Shift the camera returns to the hero.
- **Screen shake** (player `+0x534`, and a random ±8 px mode) moves the view a few pixels.
- Cut-scene cameras are not checked.

In these cases the circle check measures from the screen center, not from the hero.

## What the mod does (`src/features/movement.*`)

- **Walk radius (`[Movement] WalkRadius`):** the nine bytes that compute 3 × height plus the rounding of the division (`lea` / `cdq` / `and` / `add` right after the screen center) become a load of radius × 8 plus four `nop`s; the game's following division by 8 leaves the radius, the squared comparison stays the game's. `original` writes the original bytes back. Written in memory while only the loading thread runs; the test hotkey changes it on the game thread, where the move handler runs.
- **Double click run (`[Movement] DoubleClickRun`):** a hook on the always-run getter `0x4A6FA0` (called only by the click move `0x5173E0`) answers "run" while the last press of the left button was the second of a double click (Windows' double-click time and box, seen by the mod's `WH_GETMESSAGE` hook, `input.md`). Item clicks read `+0x480` directly (`0x4A79F0`) and are not covered.
- Neither touches AI, pathfinding or keyboard movement.
- The hero stops when a held button is released; the human wants it that way, so the mod leaves it.

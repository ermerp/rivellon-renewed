# Verified input sequences

Input for `Start-GameTest.ps1 -Steps` (Windows) and `deck.sh … steps` (Deck) – real input, so **only when the human has said the PC is free**. Coordinates are fractions of the game window's client area. Script options: [workflow.md](workflow.md).

## Driving the game by keyboard

- Every game menu (main menu, load/save list, …) works with arrow up/down plus Enter, so a save can be loaded without the mouse.
- After the start the game reacts to nothing until the mouse has been moved once – move the mouse over the game window first.
- The highlighted menu entry at the start is not reliable (a lost first key once started a new game instead of the load dialog) – first `key Up 9` (the menus do not wrap, so this always lands on the top entry), then count down.
- "New game" needs a mouse click on a character in character creation; keys alone do not get past it.
- Text fields (save name) take keys only after a click into them. The load dialog's name field takes typed letters, but they do nothing (as in the original) – edit keys and key repeat are tested in the save dialog.
- **No clicks in the bottom part of the window:** windowed, the game window with its title bar is 2562×1474 – taller than the 1440 screen – and the taskbar lies over its bottom edge; a click on the game's icon bar (y ≈ 0.96) opened the Start menu. Use the game's keys instead (`keylist.txt` in the copy lists them, e.g. **Q** = diary/quests, **I** = inventory, **M** = map, **T** = status).
- Waiting times (DE): the main menu takes input about 10 s after the start (`-NoIntro`), a loaded save about 12 s after Enter – verified by the diary run; the tables below still have the older, longer waits. The first frame comes about 8.5 s after the game window appears.
- Test saves: `C:\dev\testsaves\de\`, one per situation (`T-Brief-Buch-Inventar`, `T-Dialog-links`, `T-Händler-links`, `T-Nebel`, `T-draussen`, `T-Trank-F2` = wounded hero with a healing potion on quick slot F2). Copy them into the copy's `savegames\`, never over the human's own saves. The load list is sorted by date, newest first.
- Reading windows (inventory, letter, book) open at places stored in the save: in `T-Brief-Buch-Inventar` the EN (and FR) letter opens top left, so the DE close click walks the hero out there.

## Windows, 2560×1440 windowed (DE and EN, `-NoIntro`)

The in-game menu starts at "Continue", "Save" is the 4th entry.

| Goal | `-Steps` |
|---|---|
| Load dialog | `'wait 17','mouse 0.5 0.9','wait 1','key Up 9','key Down','key Enter','wait 3'` |
| … then load the newest save | `'key Enter','wait 25'` |
| … then the save dialog, name field focused | `'key Esc','wait 2','mouse 0.5 0.9','wait 1','key Up 9','key Down 3','key Enter','wait 2','click 0.508 0.6056'` |
| Leave the save dialog without saving | `'key Esc'` |
| Letter and books (`T-Brief-Buch-Inventar`, after loading it): inventory, chest tab "Miscellaneous", then letter / book 1 / book 2 | `'key I','wait 2','click 0.3402 0.2986','wait 1'`, then `'click 0.2508 0.3354'` / `'click 0.2727 0.3340'` / `'click 0.2934 0.3368'` |
| … next page / close (where the reading window opened in the middle) | `'click 0.4117 0.5972'` / `'click 0.3602 0.5979'` |
| Diary (quests page) of the newest save (DE) | `'wait 10','mouse 0.5 0.8','wait 1','key Up 9','key Down','key Enter','wait 3','key Enter','wait 12','mouse 0.5 0.5','wait 1','key Q','wait 3','shot diary'` |
| New game (character creation, then the first room) | `'wait 17','mouse 0.5 0.9','wait 1','key Up 9','key Enter','wait 6','click 0.474 0.466','wait 2','click 0.680 0.733'` (3rd character, then ✓; the first room is visible after about 3 s) |
| NPC dialog (`T-Dialog-links`, 8th in the load list – from the main menu; reloading in game asks for confirmation) | load dialog, `'key Down 7','key Enter','wait 25','click 0.02 0.5','wait 6'` (a click at the left edge walks the hero into the dialog) |
| Options → performance page; fog row left arrow (off) / right arrow (on); apply | `'key Down 2','key Enter','wait 3','key Up 9','key Enter','wait 3','key Up 9','key Down','key Enter'`; `'click 0.5156 0.5855'` / `'click 0.6897 0.5855'`; `'click 0.6027 0.7234'` – the keys move nothing on this page, and Esc does not leave it |
| Walk/run in `T-draussen` (3rd in the load list) | load dialog, `'key Down 2','key Enter','wait 25'`, then `'press 0.5 0.4 1500'` (144 px from the center), `'press 0.5 0.3 1500'` (288 px), `'press 0.5 0.12 1500'` (547 px), `'click 0.62 0.5'`, `'dclick 0.38 0.5'` |
| Key repeat in the save name | save dialog as above, `'type abcdefghijkl'`, `'hold Back 1000'`, `'hold Left 1000'`, `'key Right'`, `'key Del'` |
| Window state around Alt+Tab (full screen) | `'wait 16','state menu','mouse','key F10','wait 2','key 0x12','wait 2','alttab','wait 3','state away','refocus','wait 3','state back'` |

Full screen (`-FullScreen`, borderless): the "New game" clicks hit unchanged (the client area is 2560×1440 in both modes); the other rows are not yet run in full screen (planned with the release check, `PLAN.md`).

## Windows, 1600×900 windowed (DE and EN)

The main menu skips disabled entries: from the top `key Up 9` lands on "New", `Down` = Load, `Down 2` = Options, `Down 4` = Credits. Esc leaves neither an options page nor an open letter – Esc in game opens the game menu.

| Goal | `-Steps` (after `'wait 15','mouse 0.5 0.9','wait 1','key Up 9'`) |
|---|---|
| Options menu / picture settings | `'key Down 2','key Enter','wait 3'` / then `'key Up 9','key Enter','wait 3','key Up 9','key Enter','wait 3'` |
| Key configuration | `'key Down 2','key Enter','wait 3','key Up 9','key Down 3','key Enter','wait 3'` |
| Credits | `'key Down 4','key Enter','wait 5'` |
| Trader dialog and trade window (`T-Händler-links`, 7th in the load list) | `'key Down','key Enter','wait 3','key Down 6','key Enter','wait 14','mouse 0.5 0.5','key Space','wait 1','click <trader>','wait 3'`, then `'click 0.355 0.867'` (hand icon = trade). Space pauses the game right after loading; the trader still stands in different spots between runs (`0.406 0.428` or `0.469 0.433` – check with a `shot` first); without the mod the save loads faster (`wait 10`). |
| Skill panel / diary map (any loaded save) | `'click 0.40 0.978'` / `'click 0.4526 0.937'`; the icon bar's Divinity logo (`0.459 0.978`) opens the game menu |
| Inventory chest tab / letter / book 1 | `'click 0.5444 0.4778'` / `'click 0.4013 0.5367'` / `'click 0.4363 0.5344'` (the inventory sits at the same absolute pixels as at 2560×1440) |
| Letter (`T-Brief-Buch-Inventar`): close it | `'click 0.3252 0.5675'` (its ✕) |

## Steam Deck, 1280×720 (Game Mode, `deck.sh … steps`)

Steps in `tools/testrun/deck-steps.sh`. A click must hold the button for a moment (an instant down/up is ignored by the game). Picture area 800×600 in the middle for the menus. The inventory, letter and book open at places stored in the save – navigate step by step with a shot after each action.

| Goal | `steps` |
|---|---|
| Skip the intro | `key Escape wait 3 key Escape wait 3 key Escape wait 6 mouse 0.5 0.95` |
| Main menu entries (from the top: `key Up*9`; disabled entries are skipped) | Neu / Laden / Optionen at `click 0.5 0.504` / `0.5 0.56` / `0.5 0.671`; in game (all entries active) `key Up*9 key Down*2 key Return` = Laden, `Down*3` = Sichern, `Down*7` = Beenden |
| Options page row 1 / 2 / 3; Abbrechen; Zurück | `click 0.5 0.449` / `0.5 0.504` / `0.5 0.56`; `click 0.344 0.8375`; `click 0.5 0.8375` |
| Load/save list row n (n = 1 at the top) | `click 0.345 <(199 + 17·(n−1)) / 720>`, then ✓ `click 0.609 0.7125`; "really load?" ✓ `click 0.476 0.529`; save name field `click 0.445 0.7125` |
| Quit (menu "Beenden", then ✓) | `click 0.5 0.840`, `click 0.476 0.521` |
| `T-Brief-Buch-Inventar` (T1.26-D save state): chest tab / letter / close / book 1 | `key i`, `click 0.673 0.6` / `click 0.4945 0.6736` / `click 0.399 0.686` / `click 0.538 0.671` |
| Diary / close it | `key q` / `click 0.770 0.851` |
| NPC dialog (`T-Dialog-links`) / close it | `click 0.02 0.5 wait 6` / `click 0.309 0.951` |

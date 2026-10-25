# Developer flag (settings object `+0x28`)

Source of the lead: Steam forum (atom0s, 2020) – changing one byte in `div.exe` 1.0062A at file offset `0x8CCD6` (`0x5E` → `0x7E`) enables a debug mode with windowed mode and full frame rate. We never patch the EXE (rule 3); this is what the byte does.

- The pattern `89 5E 28 89 7E 2C 89 5E 34 89 5E` is unique (file `0x8CCD5`, VA `0x48D8D5`) and lies in the constructor of the settings object (`config-files.md`). The byte is the ModRM of the store that sets the default of field `+0x28`; the patch swaps the source register from the one holding 0 to the one holding 1. Nothing else changes.
- **No config key sets `+0x28`** (not `debug on` either), and no write through the global settings pointer was found. In the retail build the flag is effectively always 0.
- It is read in about **100 places** → a central developer-mode switch. Known so far:
  - startup passes it to the window/display creation (`0x557D00`, together with `+0x84`) – consistent with windowed mode;
  - if it is 0 the game hides the Windows cursor (`ShowCursor(FALSE)` at `0x40D9E2`);
  - in the main loop it skips a block with the 640×480/800×600 display setup (`0x40CB47`);
  - most other checks are clustered at `0x431000`–`0x434000` (not identified; candidate: debug key handling, see `key-events.md`). The frame-rate effect is not located yet.
- **Plan:** optional `[Debug] DeveloperMode=0` in `RivellonRenewed.ini`, setting `+0x28` in memory right after the constructor (hook it or the creation at `0x40C160`) – only once the ~100 checks are mapped.

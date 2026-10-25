# Developer workflow

## Automated start test (`tools/testrun/Start-GameTest.ps1`)

Starts the game from a working copy, takes screenshots and closes it again. Screenshots go to `C:\dev\screenshots` (outside the repo, never committed).

```powershell
# menu check without intro, two screenshots at 15 s and 25 s
.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test -NoIntro -Seconds 15 -Count 2 -Interval 10
# skip the intro with Esc
.\tools\testrun\Start-GameTest.ps1 -GameDir C:\dev\dd-test -PressEscapeAt 4,8,12,16,20 -Seconds 6 -Count 4 -Interval 7
# both working copies
foreach ($dir in 'C:\dev\dd-test', 'C:\dev\dd-test-en') {
    Install-DdMod -GameDir $dir
    .\tools\testrun\Start-GameTest.ps1 -GameDir $dir -NoIntro -Seconds 15
}
```

- Windowed by default (temporarily sets `FullScreenMode = false` in `dgVoodoo.conf`); `-FullScreen` disables that.
- `-NoIntro` temporarily sets `startup logo 0`; `-Intro` temporarily sets `startup logo 1` in `config.div` and, where present, in `config.lcl` (overrides `config.div`; the English copy ships `startup logo 0` there) – use it so DE and EN run under the same conditions. `-PressEscapeAt` posts Esc at the given seconds.
- `-PostKeyAt` posts `-PostKeyVk` (default F3, no modifiers) to the game window – no real input, safe while someone else uses the PC; enough to test a modifier-less hotkey.
- `-PressHotkeyAt` types the `-HotkeyVk` chord (default Ctrl+Shift+F3) as real input via `SendInput` at the given seconds – only if the game window could be brought to the foreground, otherwise it reports "NOT sent". Do not touch mouse or keyboard during the run.
- `-Steps` runs an input script instead of the timed screenshots (`wait`, `mouse`, `click`/`rclick`/`dclick`, `press` (left button held), `key`, `hold`, `down`/`up` (only the key-down or key-up, e.g. a shot while a key is down), `chord` (keys together, e.g. `chord Ctrl+Shift+F2`), `type`, `shot`, `state` (window style, size, focus), `alttab`, `refocus` (back like a taskbar click); syntax at the top of the script) – real input like `-PressHotkeyAt`, so **only when the human has said the PC is free**; the script stops at the first step that cannot reach the game window. Held keys (`hold`) repeat their key-down like a real keyboard (injected input has no auto-repeat of its own). Verified sequences: [test-sequences.md](test-sequences.md).
- Restores `dgVoodoo.conf`, `config.div` (with `-NoIntro`) and the run markers `init.cfg`/`sinfo.000` afterwards, so the "not correctly terminated" dialog does not appear on the next start. If it appears anyway, the script answers "No" (never "Yes", which resets the configuration).
- Reports early exit (crash) with exit code; details of a crash: Windows event log, Application, event ID 1000.
- Refuses to run on the original Steam folder.
- Captures **only the game's own window**. If the game has exited or has no window, the shot is skipped (never a desktop capture – the desktop may show private data).

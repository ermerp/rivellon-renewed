# Input and the game window (1.0062A)

## How the game reads input

- No DirectInput. `div.exe` runs its own message loop (`PeekMessageA`/`GetMessageA` → `TranslateMessage` → `DispatchMessageA`) and also polls `GetAsyncKeyState`/`GetKeyState`/`GetCursorPos`.
- Key names in bindings come from `MapVirtualKeyA`/`GetKeyNameTextA`; the bindings themselves live in `keylist.txt` (`key-events.md`).
- Default bindings with modifiers (DE and EN identical): `Ctrl+F12` key plate, `Ctrl+F11` screenshot, `Ctrl+L` quick load, `Ctrl+S` quick save. Everything else is a plain key (letters, Space, `-`).

## The game window

- `div.exe` does not create the window. The renderer plugin does (`slash1`–`4.dll` import `RegisterClassExA`/`CreateWindowExA`, `architecture.md`). All four use the window class **`Divinity`**; the title is also `Divinity`.
- It is an ANSI window (`RegisterClassExA`) and belongs to the **main thread**, the same thread that runs `DllMain` of ddmod.
- With `startup logo 0` it appears about 8.5–9 s after process start (dgVoodoo2, Windows). The intro videos play in this window. `binkw32.dll` registers a window class of its own too, so ddmod matches on the class name, not on "first visible window".
- The game window is destroyed only at exit (seen as `WM_NCDESTROY` when the test script closes the game). Whether a resolution or renderer change re-creates it: not observed yet.

## How ddmod hooks input

**Deferred start.** `DllMain` only sets up logging, config, the EXE check and starts a **watcher thread**. The thread polls every 50 ms (`EnumWindows`: own process, no owner, class `Divinity`). When the window exists, it initializes MinHook and installs the input hook. Reasons:

- Nothing to hook before the window exists; the renderer DLL is not even loaded at `DllMain` time.
- `DllMain` runs under the loader lock; enabling hooks suspends threads and should not happen there. Starting a thread from `DllMain` is safe because it only runs after the lock is released.
- Polling needs no hook to find the window. The alternatives would be: hooking `CreateWindowExA` (needs a hook in `DllMain`, or an import hook in each `slash*.dll`); a `WH_CBT` hook (also installed in `DllMain`). The 50 ms delay is irrelevant for hotkeys.
- Later game hooks (overlay at `DllSlashedEndFrame`) start from the same point.

If the window is destroyed, the hook removes itself and the watcher starts again, in case a new window appears.

**Window procedure subclassing.** `SetWindowLongPtrA(GWLP_WNDPROC)` from the watcher thread (allowed for windows of the own process). The A variant keeps the window ANSI, so the game still gets ANSI `WM_CHAR`. Our procedure:

- Handles `WM_KEYDOWN` **and** `WM_SYSKEYDOWN` (F10 and every Alt combination arrive as `WM_SYSKEYDOWN`). Modifiers come from `GetKeyState` (the state at the time of the message) and must match exactly.
- On a match: runs the handler on the game's window thread and does not pass the message on. Also swallowed: its auto-repeats, its key-up and the `WM_CHAR`/`WM_SYSCHAR` that `TranslateMessage` already produced from it (matched by scan code). Swallowing `WM_SYSKEYDOWN` F10 also prevents the window menu activation by `DefWindowProc`.
- Every other message goes to the original procedure unchanged (`CallWindowProcA`).

**Limit:** the game polls `GetAsyncKeyState` as well. A hotkey key held down is therefore still visible to that polling, even though its messages are swallowed. Hence hotkeys should be combinations the game does not use. Hooking `GetAsyncKeyState` is possible (USER32 import) if a feature needs it.

**Test hotkey:** `Ctrl+Shift+F10`. It is free in the game's bindings and the 40 key events. It also avoids Steam's F12, the tester's F5–F9 and the game's `Ctrl+F11`/`Ctrl+F12`.

## Key syntax in `ddmod.ini`

`[Keys]`: optional modifiers `Ctrl` (alias `Control`), `Shift`, `Alt` joined with `+`, then exactly one key: `A`–`Z`, `0`–`9`, `F1`–`F24`, `Num0`–`Num9`, `NumAdd`/`NumSub`/`NumMul`/`NumDiv`/`NumDecimal`, `Space`, `Enter`, `Escape`, `Tab`, `Backspace`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, arrows (`Up`, …), `Pause`, `ScrollLock` (plus short aliases such as `Esc`, `PgDn`). Case-insensitive. Empty or `None` = unbound. `PrintScreen` is not allowed (Windows sends no key-down for it). Invalid text disables the hotkey with a warning in the log. Parser: `src/input/keys.cpp`, unit tests in `tests/main.cpp`.

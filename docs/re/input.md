# Input and the game window (1.0062A)

## How the game reads input

- No DirectInput. `div.exe` runs its own message loop (`PeekMessageA`/`GetMessageA` → `TranslateMessage` → `DispatchMessageA`) and also polls `GetAsyncKeyState`/`GetKeyState`/`GetCursorPos`.
- Key names in bindings come from `MapVirtualKeyA`/`GetKeyNameTextA`; the bindings themselves live in `keylist.txt` (`key-events.md`).
- Default bindings with modifiers (DE and EN identical): `Ctrl+F12` key plate, `Ctrl+F11` screenshot, `Ctrl+L` quick load, `Ctrl+S` quick save. Everything else is a plain key (letters, Space, `-`).

## The game window

- `div.exe` does not create the window. The renderer plugin does (`slash1`–`4.dll` import `RegisterClassExA`/`CreateWindowExA`, `architecture.md`). All four use the window class **`Divinity`**; the title is also `Divinity`.
- It is an ANSI window (`RegisterClassExA`) and belongs to the **main thread**, the same thread that runs `DllMain` of ddmod.
- When it appears depends on the intro: with intro movies about 1.1–1.8 s after process start (they play in this window), with `startup logo 0` about 7.4–9.4 s (dgVoodoo2, Windows, windowed and full screen alike). `binkw32.dll` registers a window class of its own too, so ddmod matches on the class name, not on "first visible window".
- Other top-level windows of the process (full screen, dgVoodoo2): `D3DProxyWindow` and `DXGIWatchdogThreadWindow` (DXGI, hidden) and the IME windows.
- **The window procedure does not stay put.** In full screen, within 500 ms after ddmod subclassed it, the procedure pointed first into `DDRAW.dll` (dgVoodoo2) and then to code outside every module (`03BA0000`/`03CA0000`, generated at run time), and that chain does not call ours. Key messages then bypass a subclass entirely. Windowed, the subclass stayed in place.
- Alt+Tab out of full screen and back turns the game into a framed window – also without ddmod (dgVoodoo2/game behavior, not ours). Playing continues normally.
- The game window is destroyed only at exit. Whether a resolution or renderer change re-creates it: not observed yet.

## How ddmod hooks input

**Deferred start.** `DllMain` only sets up logging, config, the EXE check and starts a **watcher thread**. The thread polls every 50 ms (`EnumWindows`: own process, no owner, class `Divinity`). When the window exists, it initializes MinHook and installs the input hook. Reasons:

- Nothing to hook before the window exists; the renderer DLL is not even loaded at `DllMain` time.
- `DllMain` runs under the loader lock; enabling hooks suspends threads and should not happen there. Starting a thread from `DllMain` is safe because it only runs after the lock is released.
- Polling needs no hook to find the window. The alternatives would be: hooking `CreateWindowExA` (needs a hook in `DllMain`, or an import hook in each `slash*.dll`); a `WH_CBT` hook (also installed in `DllMain`). The 50 ms delay is irrelevant for hotkeys.
- Later game hooks (overlay at `DllSlashedEndFrame`) start from the same point.

The watcher thread stays alive afterwards: Windows removes a `SetWindowsHookEx` hook when the thread that installed it exits. With `LogLevel=debug` it logs all windows of the process with the module of their window procedure for 60 s (whenever that list changes).

**`WH_GETMESSAGE` hook on the game thread.** `SetWindowsHookExA(WH_GETMESSAGE, …, nullptr, <thread of the game window>)`. Windows calls the hook on the game thread inside `GetMessageA`/`PeekMessageA`, before the game's loop calls `TranslateMessage`/`DispatchMessageA`. It does not depend on which window procedure is current, nor on which window of the thread gets the message – important with dgVoodoo2 (Windows) and WineD3D (Deck), which both take over the window.

- Handles `WM_KEYDOWN` **and** `WM_SYSKEYDOWN` (F10 and every Alt combination arrive as `WM_SYSKEYDOWN`). Modifiers come from `GetKeyState` (the state up to the retrieved message) and must match exactly.
- On a match: runs the handler on the game thread and rewrites the message to `WM_NULL`. Its auto-repeats and its key-up are rewritten too. Because this happens before `TranslateMessage`, no `WM_CHAR` is produced and `DefWindowProc` never sees the key (no menu mode on F10).
- Only messages removed from the queue (`PM_REMOVE`) are handled; a `PM_NOREMOVE` peek comes back later with `PM_REMOVE`. Sent (not posted) messages do not pass the hook; keyboard input is always posted.
- Every other message passes unchanged.

Rejected: subclassing the window procedure (`SetWindowLongPtrA(GWLP_WNDPROC)`) – worked windowed, but in full screen dgVoodoo2 replaces the procedure without chaining to ours (see above).

**Limit:** the game polls `GetAsyncKeyState` as well. A hotkey key held down is therefore still visible to that polling, even though its messages are swallowed. Hence hotkeys should be combinations the game does not use. Hooking `GetAsyncKeyState` is possible (USER32 import) if a feature needs it.

**Rule for default hotkeys:** no F10 and no Alt combinations. If the hook ever misses a key, Windows treats F10/Alt as the menu key and puts the game window into menu mode – the game seems to freeze until a click (seen in T1.5-W part B with `Ctrl+Shift+F10`). Also avoid the game's bindings, Steam's F12 and the tester's F5–F9.

**Test hotkey:** `Ctrl+Shift+F3` – free in the game's bindings and the 40 key events.

## Key syntax in `ddmod.ini`

`[Keys]`: optional modifiers `Ctrl` (alias `Control`), `Shift`, `Alt` joined with `+`, then exactly one key: `A`–`Z`, `0`–`9`, `F1`–`F24`, `Num0`–`Num9`, `NumAdd`/`NumSub`/`NumMul`/`NumDiv`/`NumDecimal`, `Space`, `Enter`, `Escape`, `Tab`, `Backspace`, `Insert`, `Delete`, `Home`, `End`, `PageUp`, `PageDown`, arrows (`Up`, …), `Pause`, `ScrollLock` (plus short aliases such as `Esc`, `PgDn`). Case-insensitive. Empty or `None` = unbound. `PrintScreen` is not allowed (Windows sends no key-down for it). Invalid text disables the hotkey with a warning in the log. Parser: `src/input/keys.cpp`, unit tests in `tests/main.cpp`.

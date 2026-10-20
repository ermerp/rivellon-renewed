<#
.SYNOPSIS
    Starts Divine Divinity from a working copy, takes screenshots and closes the game again.

.DESCRIPTION
    - Refuses to run from the original Steam folder.
    - Windowed by default: temporarily sets FullScreenMode = false in dgVoodoo.conf and restores
      that key (and AppControlledScreenMode) afterwards (requires dgVoodoo2 in the working copy).
    - Takes -Count screenshots, the first after -Seconds, then every -Interval seconds.
    - -NoIntro temporarily sets "startup logo 0" in config.div (skips the intro movies) and
      restores config.div afterwards. -Intro does the opposite ("startup logo 1" in config.div and,
      if the key is there, in config.lcl, which overrides config.div) so DE and EN run alike.
    - -PostKeyAt posts -PostKeyVk (default F3) to the game window without modifiers - no real input.
    - -PressHotkeyAt types the -HotkeyVk chord as real input (SendInput, so GetKeyState sees the
      modifiers), but only if the game window is in the foreground - never into another program.
    - -Steps runs an input script instead of the -Count screenshots, one step per entry, in order
      (real input like -PressHotkeyAt, only into the foreground game window):
        wait <s>              wait (fractions allowed, e.g. 0.5)
        mouse [<fx> <fy>]     cursor to that point of the window (fractions, default 0.5 0.5) and
                              a small move - the menus take arrow keys only after a mouse move
        click <fx> <fy>       left click at that point of the window (fractions of the client area);
                              rclick = right click, dclick = double click
        key <name> [<n>]      press a key n times (default 1); names: Up Down Left Right Enter Esc
                              Back Tab Space Home End Del F1-F12, a letter or digit, or 0xNN
        hold <name> <ms>      hold a key, with key-down repeats like a held real key
        type <text>           letters, digits and spaces as key presses
        shot <name>           screenshot of the game window
      Example: -Steps 'wait 20','mouse','key Down 2','key Enter','wait 3','shot load'
    - Saves PNGs to -OutDir (default C:\dev\screenshots, outside the repo).
    - Reports whether the game exited early (crash) or stopped responding.
    - The script has to kill the game, which the game would treat as a crash on the next start
      ("Divinity was not correctly terminated ... reset the configuration?"). To avoid that, the
      run marker files (init.cfg, sinfo.000) are restored after each run. If the dialog appears
      anyway, it is captured and answered with "No" - never "Yes", which would reset config.div.

.EXAMPLE
    .\Start-GameTest.ps1 -GameDir C:\dev\dd-test -Seconds 15 -Count 3 -Interval 10
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $GameDir,
    [int] $Seconds = 15,
    [int] $Count = 1,
    [int] $Interval = 10,
    [switch] $FullScreen,
    [switch] $NoIntro,
    [switch] $Intro,                # force the intro on (the English copy disables it in config.lcl)
    [int[]] $PressEscapeAt = @(),   # seconds after start at which Esc is sent (e.g. to skip videos)
    [int[]] $PostKeyAt = @(),       # seconds after start at which -PostKeyVk is posted (no modifiers, no real input)
    [int] $PostKeyVk = 0x72,        # F3
    [int[]] $PressHotkeyAt = @(),   # seconds after start at which the -HotkeyVk chord is typed
    [int[]] $HotkeyVk = @(0x11, 0x10, 0x72),   # virtual keys, pressed in order: Ctrl+Shift+F3
    [string[]] $Steps = @(),        # input script run instead of the -Count screenshots (see description)
    [string] $OutDir = 'C:\dev\screenshots',
    [string] $Label
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'DdTest.psm1') -Force

Add-Type -AssemblyName System.Drawing
Add-Type -TypeDefinition @'
using System;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Collections.Generic;

public static class DdCapture {
    [DllImport("user32.dll")] static extern bool SetProcessDpiAwarenessContext(IntPtr v);
    [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc cb, IntPtr p);
    [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
    [DllImport("user32.dll")] static extern bool IsWindowVisible(IntPtr h);
    [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
    [DllImport("user32.dll")] static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("dwmapi.dll")] static extern int DwmGetWindowAttribute(IntPtr h, int attr, out RECT r, int size);
    delegate bool EnumProc(IntPtr h, IntPtr p);

    [StructLayout(LayoutKind.Sequential)]
    public struct RECT { public int Left, Top, Right, Bottom; }

    [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowText(IntPtr h, System.Text.StringBuilder s, int n);
    [DllImport("user32.dll")] static extern IntPtr SendMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);

    public static void InitDpi() { SetProcessDpiAwarenessContext(new IntPtr(-4)); }

    // Visible top-level window of the process with exactly this title, or IntPtr.Zero.
    public static IntPtr FindWindowByTitle(uint pid, string title) {
        IntPtr found = IntPtr.Zero;
        EnumWindows((h, p) => {
            uint wpid; GetWindowThreadProcessId(h, out wpid);
            if (wpid == pid && IsWindowVisible(h)) {
                var sb = new System.Text.StringBuilder(256);
                GetWindowText(h, sb, sb.Capacity);
                if (sb.ToString() == title) { found = h; return false; }
            }
            return true;
        }, IntPtr.Zero);
        return found;
    }

    [DllImport("user32.dll")] static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);

    // Posts a key press (down + up) for a virtual key to the window.
    public static void PostKey(IntPtr h, int vk) {
        PostMessage(h, 0x0100 /* WM_KEYDOWN */, new IntPtr(vk), new IntPtr(1));
        System.Threading.Thread.Sleep(50);
        PostMessage(h, 0x0101 /* WM_KEYUP */, new IntPtr(vk), new IntPtr(unchecked((int)0xC0000001)));
    }

    [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] static extern bool AttachThreadInput(uint a, uint b, bool attach);
    [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
    [DllImport("user32.dll")] static extern uint SendInput(uint n, INPUT[] inputs, int size);

    [StructLayout(LayoutKind.Sequential)]
    struct KEYBDINPUT { public ushort vk, scan; public uint flags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Sequential)]
    struct MOUSEINPUT { public int dx, dy; public uint data, flags, time; public IntPtr extra; }
    [StructLayout(LayoutKind.Explicit)]
    struct INPUTUNION { [FieldOffset(0)] public MOUSEINPUT mi; [FieldOffset(0)] public KEYBDINPUT ki; }
    // Sequential layout gives the right union offset and size for 32- and 64-bit PowerShell.
    [StructLayout(LayoutKind.Sequential)]
    struct INPUT { public uint type; public INPUTUNION u; }

    // Types a key chord (keys down in order, up in reverse order) as real input, but only if the
    // window is in the foreground. Returns false (nothing sent) otherwise.
    public static bool SendChord(IntPtr h, int[] vks) {
        // A background process may not take the foreground; sharing the input state with the
        // current foreground thread allows it without sending any input there.
        uint dummy;
        uint fgThread = GetWindowThreadProcessId(GetForegroundWindow(), out dummy);
        uint self = GetCurrentThreadId();
        bool attached = fgThread != 0 && fgThread != self && AttachThreadInput(self, fgThread, true);
        SetForegroundWindow(h);
        if (attached) AttachThreadInput(self, fgThread, false);
        System.Threading.Thread.Sleep(300);
        if (GetForegroundWindow() != h) return false;
        var inputs = new INPUT[vks.Length * 2];
        for (int i = 0; i < vks.Length; i++) {
            inputs[i].type = 1; inputs[i].u.ki.vk = (ushort)vks[i];
            var up = vks.Length * 2 - 1 - i;
            inputs[up].type = 1; inputs[up].u.ki.vk = (ushort)vks[i]; inputs[up].u.ki.flags = 2 /* KEYEVENTF_KEYUP */;
        }
        return SendInput((uint)inputs.Length, inputs, Marshal.SizeOf(typeof(INPUT))) == inputs.Length;
    }

    [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h, out RECT r);
    [DllImport("user32.dll")] static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] static extern bool SetCursorPos(int x, int y);
    [StructLayout(LayoutKind.Sequential)]
    public struct POINT { public int X, Y; }

    // Brings the window to the foreground (see SendChord); false if that failed.
    static bool Focus(IntPtr h) {
        if (GetForegroundWindow() == h) return true;
        uint dummy;
        uint fgThread = GetWindowThreadProcessId(GetForegroundWindow(), out dummy);
        uint self = GetCurrentThreadId();
        bool attached = fgThread != 0 && fgThread != self && AttachThreadInput(self, fgThread, true);
        SetForegroundWindow(h);
        if (attached) AttachThreadInput(self, fgThread, false);
        System.Threading.Thread.Sleep(300);
        return GetForegroundWindow() == h;
    }

    static bool SendKeyEvent(int vk, bool up) {
        var inputs = new INPUT[1];
        inputs[0].type = 1; inputs[0].u.ki.vk = (ushort)vk; inputs[0].u.ki.flags = up ? 2u : 0u;
        return SendInput(1, inputs, Marshal.SizeOf(typeof(INPUT))) == 1;
    }

    // Presses a key as real input and holds it for holdMs. Injected input has no hardware
    // auto-repeat, so held keys repeat the key-down like Windows does by default (first repeat
    // after 500 ms, then about 30 per second). Nothing is sent unless the window has the focus.
    public static bool SendKey(IntPtr h, int vk, int holdMs) {
        if (!Focus(h)) return false;
        if (!SendKeyEvent(vk, false)) return false;
        var watch = System.Diagnostics.Stopwatch.StartNew();
        long next = 500;
        while (watch.ElapsedMilliseconds < holdMs) {
            System.Threading.Thread.Sleep(5);
            if (watch.ElapsedMilliseconds >= next && watch.ElapsedMilliseconds < holdMs) {
                if (GetForegroundWindow() != h) break;
                SendKeyEvent(vk, false);
                next += 33;
            }
        }
        if (holdMs < 50) System.Threading.Thread.Sleep(50);
        return SendKeyEvent(vk, true);
    }

    // Puts the cursor on the client area at (fx, fy) as fractions of its size and moves it a
    // little as real input, so the game sees a mouse move. Nothing happens without the focus.
    public static bool MoveMouse(IntPtr h, double fx, double fy) {
        if (!Focus(h)) return false;
        RECT r; GetClientRect(h, out r);
        var p = new POINT { X = (int)(r.Right * fx), Y = (int)(r.Bottom * fy) };
        ClientToScreen(h, ref p);
        SetCursorPos(p.X, p.Y);
        var inputs = new INPUT[2];
        inputs[0].u.mi.dx = 4; inputs[0].u.mi.dy = 4; inputs[0].u.mi.flags = 1 /* MOUSEEVENTF_MOVE */;
        inputs[1].u.mi.dx = -4; inputs[1].u.mi.dy = -4; inputs[1].u.mi.flags = 1;
        bool ok = SendInput(1, new[] { inputs[0] }, Marshal.SizeOf(typeof(INPUT))) == 1;
        System.Threading.Thread.Sleep(50);
        return ok && SendInput(1, new[] { inputs[1] }, Marshal.SizeOf(typeof(INPUT))) == 1;
    }

    // Click at (fx, fy) of the client area (fractions of its size) as real input: left (or right)
    // button, `count` times (2 = double click).
    public static bool Click(IntPtr h, double fx, double fy, bool right, int count) {
        if (!MoveMouse(h, fx, fy)) return false;
        System.Threading.Thread.Sleep(100);
        var inputs = new INPUT[1];
        bool ok = true;
        for (int i = 0; i < count && ok; i++) {
            inputs[0].u.mi.flags = right ? 8u /* RIGHTDOWN */ : 2u /* LEFTDOWN */;
            ok = SendInput(1, inputs, Marshal.SizeOf(typeof(INPUT))) == 1;
            System.Threading.Thread.Sleep(60);
            inputs[0].u.mi.flags = right ? 16u /* RIGHTUP */ : 4u /* LEFTUP */;
            ok = ok && SendInput(1, inputs, Marshal.SizeOf(typeof(INPUT))) == 1;
            System.Threading.Thread.Sleep(60);
        }
        return ok;
    }

    // Answers a standard message box with the given button id (e.g. 7 = IDNO).
    public static void AnswerMessageBox(IntPtr h, int buttonId) {
        SendMessage(h, 0x0111 /* WM_COMMAND */, new IntPtr(buttonId), IntPtr.Zero);
    }

    // Largest visible top-level window of the process, or IntPtr.Zero.
    public static IntPtr FindMainWindow(uint pid) {
        IntPtr best = IntPtr.Zero; long bestArea = 0;
        EnumWindows((h, p) => {
            uint wpid; GetWindowThreadProcessId(h, out wpid);
            if (wpid == pid && IsWindowVisible(h)) {
                RECT r; GetWindowRect(h, out r);
                long area = (long)(r.Right - r.Left) * (r.Bottom - r.Top);
                if (area > bestArea) { bestArea = area; best = h; }
            }
            return true;
        }, IntPtr.Zero);
        return best;
    }

    static RECT Bounds(IntPtr h) {
        RECT r;
        if (DwmGetWindowAttribute(h, 9 /* DWMWA_EXTENDED_FRAME_BOUNDS */, out r, Marshal.SizeOf(typeof(RECT))) != 0)
            GetWindowRect(h, out r);
        return r;
    }

    static bool IsBlack(Bitmap b) {
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++) {
                Color c = b.GetPixel(b.Width * (2 * x + 1) / 16, b.Height * (2 * y + 1) / 16);
                if (c.R > 8 || c.G > 8 || c.B > 8) return false;
            }
        return true;
    }

    // Captures a window: PrintWindow first (works when covered), screen copy as fallback - but
    // only if the window is in the foreground and smaller than its monitor. A screen copy of a
    // full-screen window would be a desktop-sized image (CLAUDE.md rule 11): skipped instead.
    public static Bitmap CaptureWindow(IntPtr h, out string method) {
        RECT r = Bounds(h);
        int w = r.Right - r.Left, ht = r.Bottom - r.Top;
        if (w <= 0 || ht <= 0) { method = "none"; return null; }
        Bitmap bmp = new Bitmap(w, ht);
        using (Graphics g = Graphics.FromImage(bmp)) {
            IntPtr hdc = g.GetHdc();
            bool ok = PrintWindow(h, hdc, 2 /* PW_RENDERFULLCONTENT */);
            g.ReleaseHdc(hdc);
            if (ok && !IsBlack(bmp)) { method = "PrintWindow"; return bmp; }
        }
        var screen = System.Windows.Forms.Screen.FromHandle(h).Bounds;
        if (w >= screen.Width || ht >= screen.Height) {
            bmp.Dispose(); method = "skipped (full screen, PrintWindow black)"; return null;
        }
        SetForegroundWindow(h);
        System.Threading.Thread.Sleep(500);
        if (GetForegroundWindow() != h) {
            bmp.Dispose(); method = "skipped (game window not in the foreground)"; return null;
        }
        using (Graphics g = Graphics.FromImage(bmp))
            g.CopyFromScreen(r.Left, r.Top, 0, 0, new Size(w, ht));
        method = "CopyFromScreen";
        return bmp;
    }

}
'@ -ReferencedAssemblies System.Drawing, System.Windows.Forms

[DdCapture]::InitDpi()

$dir = Assert-WorkingCopy $GameDir
if (-not $Label) { $Label = Split-Path $dir -Leaf }
New-Item -ItemType Directory -Force $OutDir | Out-Null

$conf = Join-Path $dir 'dgVoodoo.conf'
$confBackup = $null
if (-not $FullScreen) {
    if (-not (Test-Path -LiteralPath $conf)) { throw "Windowed mode needs dgVoodoo.conf in $dir (or use -FullScreen)" }
    # Only these two keys are restored afterwards, not the whole file: the mod itself may change
    # dgVoodoo.conf during the run ([DgVoodoo] Watermark) and that change must stay.
    $confBackup = @(
        @('General', 'FullScreenMode', (Get-IniValue -Path $conf -Section 'General' -Key 'FullScreenMode')),
        @('DirectX', 'AppControlledScreenMode', (Get-IniValue -Path $conf -Section 'DirectX' -Key 'AppControlledScreenMode'))
    )
    Set-IniValue -Path $conf -Section 'General' -Key 'FullScreenMode' -Value 'false'
    Set-IniValue -Path $conf -Section 'DirectX' -Key 'AppControlledScreenMode' -Value 'false'
}
$divCfg = Join-Path $dir 'config.div'
$divCfgBackup = $null
$lclCfg = Join-Path $dir 'config.lcl'
$lclCfgBackup = $null
if ($NoIntro -and $Intro) { throw '-NoIntro and -Intro exclude each other' }
if ($NoIntro -or $Intro) {
    $divCfgBackup = [System.IO.File]::ReadAllBytes($divCfg)
    $logo = if ($Intro) { '1' } else { '0' }
    Set-DivConfigValue -Path $divCfg -Key 'startup logo' -Value $logo -Comment 'show the intro movies'
    # config.lcl overrides config.div; the English release ships "startup logo 0" there.
    if ($Intro -and (Select-String -LiteralPath $lclCfg -Pattern '^\s*startup logo\s' -Quiet)) {
        $lclCfgBackup = [System.IO.File]::ReadAllBytes($lclCfg)
        Set-DivConfigValue -Path $lclCfg -Key 'startup logo' -Value '1'
    }
}

# Snapshot of the game's run marker files (restored afterwards, see description).
$markers = @{}
foreach ($m in 'init.cfg', 'sinfo.000') {
    $p = Join-Path $dir $m
    $markers[$p] = $(if (Test-Path -LiteralPath $p) { [System.IO.File]::ReadAllBytes($p) } else { $null })
}

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$results = @()
$proc = $null
$crashDialogSeen = $false

function Save-Shot([System.Drawing.Bitmap] $bmp, [string] $suffix, [string] $method, [int] $elapsed) {
    $file = '-'
    $size = '-'
    if ($bmp) {
        $file = Join-Path $OutDir ('{0}_{1}_{2}.png' -f $Label, $stamp, $suffix)
        $bmp.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
        $size = "$($bmp.Width)x$($bmp.Height)"
        $bmp.Dispose()
    }
    $script:results += [pscustomobject]@{
        Shot = $suffix; Seconds = $elapsed; Size = $size; Method = $method
        Responding = $(if ($proc.HasExited) { '-' } else { $proc.Responding }); File = $file
    }
}

function Resolve-CrashDialog {
    # Captures and answers the "not correctly terminated" dialog with "No".
    if ($proc.HasExited) { return }
    $h = [DdCapture]::FindWindowByTitle([uint32]$proc.Id, 'Divinity - Error')
    if ($h -eq [IntPtr]::Zero) { return }
    $method = ''
    $bmp = [DdCapture]::CaptureWindow($h, [ref]$method)
    Save-Shot $bmp 'error-dialog' $method ([int]((Get-Date) - $start).TotalSeconds)
    [DdCapture]::AnswerMessageBox($h, 7)   # IDNO
    $script:crashDialogSeen = $true
    Write-Host 'Answered "not correctly terminated" dialog with "No".'
}
function Get-KeyVk([string] $name) {
    $named = @{
        Up = 0x26; Down = 0x28; Left = 0x25; Right = 0x27; Enter = 0x0D; Esc = 0x1B; Back = 0x08
        Tab = 0x09; Space = 0x20; Home = 0x24; End = 0x23; Del = 0x2E
    }
    if ($named.ContainsKey($name)) { return $named[$name] }
    if ($name -match '^F([1-9]|1[0-2])$') { return 0x6F + [int]$Matches[1] }
    if ($name -match '^0x[0-9A-Fa-f]{1,2}$') { return [Convert]::ToInt32($name.Substring(2), 16) }
    if ($name -match '^[A-Za-z0-9]$') { return [int][char]$name.ToUpperInvariant() }
    throw "Unknown key '$name' in -Steps"
}

function Wait-Game([double] $seconds) {
    $until = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $until -and -not $proc.HasExited) {
        Resolve-CrashDialog
        Start-Sleep -Milliseconds 100
    }
}

# Runs one -Steps entry; returns $false if input could not be sent (game window not in front).
function Invoke-Step([string] $step) {
    $parts = @($step.Trim() -split '\s+', 2)
    $verb = $parts[0]
    $arg = if ($parts.Count -gt 1) { $parts[1] } else { '' }
    $h = [DdCapture]::FindMainWindow([uint32]$proc.Id)
    $elapsed = '{0,6:N1}s' -f ((Get-Date) - $start).TotalSeconds
    switch ($verb) {
        'wait' { Wait-Game ([double]::Parse($arg, [Globalization.CultureInfo]::InvariantCulture)); return $true }
        'shot' {
            $method = ''
            $bmp = if ($h -ne [IntPtr]::Zero) { [DdCapture]::CaptureWindow($h, [ref]$method) } else { $method = 'skipped (no game window)'; $null }
            Save-Shot $bmp $arg $method ([int]((Get-Date) - $start).TotalSeconds)
            return $true
        }
    }
    if ($h -eq [IntPtr]::Zero) { Write-Host "$elapsed $step - no game window"; return $false }
    $ok = $true
    switch ($verb) {
        'mouse' {
            $f = @($arg -split '\s+' | Where-Object { $_ } | ForEach-Object { [double]::Parse($_, [Globalization.CultureInfo]::InvariantCulture) })
            if ($f.Count -ne 2) { $f = @(0.5, 0.5) }
            $ok = [DdCapture]::MoveMouse($h, $f[0], $f[1])
        }
        { $_ -in 'click', 'rclick', 'dclick' } {
            $f = @($arg -split '\s+' | Where-Object { $_ } | ForEach-Object { [double]::Parse($_, [Globalization.CultureInfo]::InvariantCulture) })
            if ($f.Count -ne 2) { throw "$verb needs <fx> <fy>: '$step'" }
            $ok = [DdCapture]::Click($h, $f[0], $f[1], $verb -eq 'rclick', $(if ($verb -eq 'dclick') { 2 } else { 1 }))
        }
        'key' {
            $a = @($arg -split '\s+')
            $times = if ($a.Count -gt 1) { [int]$a[1] } else { 1 }
            $vk = Get-KeyVk $a[0]
            for ($i = 0; $i -lt $times -and $ok; $i++) {
                $ok = [DdCapture]::SendKey($h, $vk, 0)
                Start-Sleep -Milliseconds 150
            }
        }
        'hold' {
            $a = @($arg -split '\s+')
            $ok = [DdCapture]::SendKey($h, (Get-KeyVk $a[0]), [int]$a[1])
        }
        'type' {
            foreach ($c in $arg.ToCharArray()) {
                $name = if ($c -eq ' ') { 'Space' } else { [string]$c }
                $ok = [DdCapture]::SendKey($h, (Get-KeyVk $name), 0)
                if (-not $ok) { break }
                Start-Sleep -Milliseconds 100
            }
        }
        default { throw "Unknown step '$step'" }
    }
    Write-Host "$elapsed $step$(if (-not $ok) { ' - NOT sent (game window not in the foreground)' })"
    return $ok
}

try {
    $proc = Start-Process -FilePath (Join-Path $dir 'div.exe') -WorkingDirectory $dir -PassThru
    $null = $proc.Handle   # keep the handle open, otherwise ExitCode stays empty in PowerShell 5.1
    Write-Host "Started div.exe (PID $($proc.Id)) from $dir"
    $start = Get-Date
    $pendingEsc = @($PressEscapeAt | Sort-Object)
    $pendingHotkey = @($PressHotkeyAt | Sort-Object)
    $pendingPost = @($PostKeyAt | Sort-Object)

    if ($Steps.Count -gt 0) {
        foreach ($step in $Steps) {
            if ($proc.HasExited) { Write-Host "Game exited before step '$step'"; break }
            # Stop at the first step that cannot be sent: later input would go elsewhere.
            if (-not (Invoke-Step $step)) { Write-Host 'Input script stopped.'; break }
        }
        $Count = 0
    }

    for ($n = 1; $n -le $Count; $n++) {
        $due = $start.AddSeconds($Seconds + ($n - 1) * $Interval)
        while ((Get-Date) -lt $due -and -not $proc.HasExited) {
            Resolve-CrashDialog
            $now = ((Get-Date) - $start).TotalSeconds
            foreach ($t in @($pendingEsc)) {
                if ($now -ge $t) {
                    $h = [DdCapture]::FindMainWindow([uint32]$proc.Id)
                    if ($h -ne [IntPtr]::Zero) { [DdCapture]::PostKey($h, 0x1B); Write-Host "Esc sent at ${t}s" }
                    $pendingEsc = @($pendingEsc | Where-Object { $_ -ne $t })
                }
            }
            foreach ($t in @($pendingPost)) {
                if ($now -ge $t) {
                    $h = [DdCapture]::FindMainWindow([uint32]$proc.Id)
                    if ($h -ne [IntPtr]::Zero) { [DdCapture]::PostKey($h, $PostKeyVk); Write-Host ("Key 0x{0:X2} posted at ${t}s" -f $PostKeyVk) }
                    $pendingPost = @($pendingPost | Where-Object { $_ -ne $t })
                }
            }
            foreach ($t in @($pendingHotkey)) {
                if ($now -ge $t) {
                    $h = [DdCapture]::FindMainWindow([uint32]$proc.Id)
                    if ($h -ne [IntPtr]::Zero -and [DdCapture]::SendChord($h, $HotkeyVk)) { Write-Host "Hotkey sent at ${t}s" }
                    else { Write-Host "Hotkey NOT sent at ${t}s (game window not in the foreground)" }
                    $pendingHotkey = @($pendingHotkey | Where-Object { $_ -ne $t })
                }
            }
            Start-Sleep -Milliseconds 250
        }

        $elapsed = [int]((Get-Date) - $start).TotalSeconds
        # Only the game's own window is ever captured - never the desktop, which may show private data.
        $bmp = $null
        if ($proc.HasExited) {
            $method = 'skipped (game exited)'
        } else {
            $proc.Refresh()
            $hwnd = [DdCapture]::FindMainWindow([uint32]$proc.Id)
            if ($hwnd -eq [IntPtr]::Zero) { $method = 'skipped (no game window)' }
            else { $method = ''; $bmp = [DdCapture]::CaptureWindow($hwnd, [ref]$method) }
        }
        Save-Shot $bmp ('{0:D2}' -f $n) $method $elapsed
        if ($proc.HasExited) { break }
    }
}
finally {
    $status = 'not started'
    if ($proc) {
        if ($proc.HasExited) {
            $status = "EXITED EARLY after $([int]($proc.ExitTime - $proc.StartTime).TotalSeconds)s, exit code $($proc.ExitCode) (0x{0:X8})" -f $proc.ExitCode
        } else {
            $status = 'running until closed by script'
            $null = $proc.CloseMainWindow()
            if (-not $proc.WaitForExit(3000)) { Stop-Process -Id $proc.Id -Force }
        }
    }
    if ($confBackup) { foreach ($s in $confBackup) { Set-IniValue -Path $conf -Section $s[0] -Key $s[1] -Value $s[2] } }
    if ($divCfgBackup) { [System.IO.File]::WriteAllBytes($divCfg, $divCfgBackup) }
    if ($lclCfgBackup) { [System.IO.File]::WriteAllBytes($lclCfg, $lclCfgBackup) }
    foreach ($p in $markers.Keys) {
        if ($null -eq $markers[$p]) { Remove-Item -LiteralPath $p -ErrorAction SilentlyContinue }
        else { [System.IO.File]::WriteAllBytes($p, $markers[$p]) }
    }
    $results | Format-Table -AutoSize | Out-String | Write-Host
    Write-Host "Game status: $status"
    if ($crashDialogSeen) { Write-Host 'Note: the "not correctly terminated" dialog appeared (answered "No").' }
    $modLog = Join-Path $dir 'RivellonRenewed.log'
    if (Test-Path -LiteralPath $modLog) {
        Write-Host '--- RivellonRenewed.log ---'
        Get-Content -LiteralPath $modLog | Write-Host
    }
}

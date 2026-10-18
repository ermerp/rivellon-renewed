<#
.SYNOPSIS
    Starts Divine Divinity from a working copy, takes screenshots and closes the game again.

.DESCRIPTION
    - Refuses to run from the original Steam folder.
    - Windowed by default: temporarily sets FullScreenMode = false in dgVoodoo.conf and restores
      the file afterwards (requires dgVoodoo2 in the working copy).
    - Takes -Count screenshots, the first after -Seconds, then every -Interval seconds.
    - -NoIntro temporarily sets "startup logo 0" in config.div (skips the intro movies) and
      restores config.div afterwards.
    - -PressHotkeyAt types the -HotkeyVk chord as real input (SendInput, so GetKeyState sees the
      modifiers), but only if the game window is in the foreground - never into another program.
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
    [int[]] $PressEscapeAt = @(),   # seconds after start at which Esc is sent (e.g. to skip videos)
    [int[]] $PressHotkeyAt = @(),   # seconds after start at which the -HotkeyVk chord is typed
    [int[]] $HotkeyVk = @(0x11, 0x10, 0x79),   # virtual keys, pressed in order: Ctrl+Shift+F10
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

    // Captures a window: PrintWindow first (works when covered), screen copy as fallback.
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
        SetForegroundWindow(h);
        System.Threading.Thread.Sleep(500);
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
    $confBackup = [System.IO.File]::ReadAllBytes($conf)
    Set-IniValue -Path $conf -Section 'General' -Key 'FullScreenMode' -Value 'false'
    Set-IniValue -Path $conf -Section 'DirectX' -Key 'AppControlledScreenMode' -Value 'false'
}
$divCfg = Join-Path $dir 'config.div'
$divCfgBackup = $null
if ($NoIntro) {
    $divCfgBackup = [System.IO.File]::ReadAllBytes($divCfg)
    Set-DivConfigValue -Path $divCfg -Key 'startup logo' -Value '0' -Comment 'show the intro movies'
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
try {
    $proc = Start-Process -FilePath (Join-Path $dir 'div.exe') -WorkingDirectory $dir -PassThru
    $null = $proc.Handle   # keep the handle open, otherwise ExitCode stays empty in PowerShell 5.1
    Write-Host "Started div.exe (PID $($proc.Id)) from $dir"
    $start = Get-Date
    $pendingEsc = @($PressEscapeAt | Sort-Object)
    $pendingHotkey = @($PressHotkeyAt | Sort-Object)

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
    if ($confBackup) { [System.IO.File]::WriteAllBytes($conf, $confBackup) }
    if ($divCfgBackup) { [System.IO.File]::WriteAllBytes($divCfg, $divCfgBackup) }
    foreach ($p in $markers.Keys) {
        if ($null -eq $markers[$p]) { Remove-Item -LiteralPath $p -ErrorAction SilentlyContinue }
        else { [System.IO.File]::WriteAllBytes($p, $markers[$p]) }
    }
    $results | Format-Table -AutoSize | Out-String | Write-Host
    Write-Host "Game status: $status"
    if ($crashDialogSeen) { Write-Host 'Note: the "not correctly terminated" dialog appeared (answered "No").' }
    $modLog = Join-Path $dir 'ddmod.log'
    if (Test-Path -LiteralPath $modLog) {
        Write-Host '--- ddmod.log ---'
        Get-Content -LiteralPath $modLog | Write-Host
    }
}

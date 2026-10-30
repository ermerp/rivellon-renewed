# Rivellon Renewed setup (Windows). Started by RivellonRenewed-Setup.cmd in the game folder; lives in
# <game>\RivellonRenewed\setup.ps1. Windows PowerShell 5.1, no other requirements.
#
#   (no switch)  menu: install dgVoodoo2 / uninstall Rivellon Renewed / quit
#   -Install     downloads dgVoodoo2 (pinned version, SHA-256 checked) and puts its DirectX files
#                into the game folder
#   -Uninstall   removes Rivellon Renewed and the dgVoodoo2 files this script installed
#   -NoPause     no "press a key" at the end (automated tests)
#
# dgVoodoo2 (by Dege, https://github.com/dege-diosg/dgVoodoo2) is downloaded from its official
# GitHub release, not shipped with the mod. Files this script did not put there are never touched.
[CmdletBinding()]
param([switch]$Install, [switch]$Uninstall, [switch]$NoPause)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2

$DgVoodooVersion = '2.87.5'
$DgVoodooUrl = 'https://github.com/dege-diosg/dgVoodoo2/releases/download/v2.87.5/dgVoodoo2_87_5.zip'
$DgVoodooZipSha256 = '5FFDE6927F7355CA3FDD5D785B581256A8E6539FA13E395A891ADE6BA1040850'
# Files taken from the archive: archive path -> name in the game folder, SHA-256.
$DgVoodooFiles = @(
    @{ From = 'MS/x86/DDraw.dll';  To = 'DDraw.dll';       Sha256 = '612A24408A090A3C6F3886557FA18034EE742E94AD0A40EBDF854D2816176C2E' },
    @{ From = 'MS/x86/D3DImm.dll'; To = 'D3DImm.dll';      Sha256 = '93C534F2D17419EA78F15551F7E0AAC78B3C503733A840914FA063708A5AFE8E' },
    @{ From = 'dgVoodooCpl.exe';   To = 'dgVoodooCpl.exe'; Sha256 = '87FB878166DCAEF3E75FF170DE030527C73F5C9B164C5943DA13F8A97B0B75C6' },
    @{ From = 'dgVoodoo.conf';     To = 'dgVoodoo.conf';   Sha256 = $null }  # the mod edits it at every start
)
$AsiLoaderSha256 = 'EC2F4824ECA58DD40F425756A4A7CEC77B8E381F21D11D7C846EC4B339B617AB'

$ModDir = $PSScriptRoot
$GameDir = Split-Path -Parent $ModDir
$Marker = Join-Path $ModDir 'dgvoodoo-installed.txt'  # files this script installed, one per line

function Say([string]$text, [string]$color = 'Gray') { Write-Host $text -ForegroundColor $color }
function Step([int]$n, [int]$of, [string]$text) { Write-Host ''; Write-Host "[$n/$of] $text" -ForegroundColor Cyan }
function Fail([string]$text) { throw $text }
function Hash([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }

function Test-GameFolder {
    if (-not (Test-Path -LiteralPath (Join-Path $GameDir 'div.exe'))) {
        Fail "div.exe not found in $GameDir. Unpack the Rivellon Renewed zip into the Divine Divinity folder (the one with div.exe) and start RivellonRenewed-Setup.cmd there."
    }
    if (Get-Process -Name div -ErrorAction SilentlyContinue) { Fail 'Divine Divinity is running. Quit the game first.' }
    Say "Game folder: $GameDir"
}

function Get-WithProgress([string]$url, [string]$file) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    $request = [Net.HttpWebRequest]::Create($url)
    $request.UserAgent = 'RivellonRenewed-Setup'
    $response = $request.GetResponse()
    $total = $response.ContentLength
    $in = $response.GetResponseStream()
    $out = [IO.File]::Create($file)
    try {
        $buffer = New-Object byte[] 65536
        $done = 0L
        $lastShown = -1
        while (($read = $in.Read($buffer, 0, $buffer.Length)) -gt 0) {
            $out.Write($buffer, 0, $read)
            $done += $read
            if ($total -gt 0) {
                $percent = [int](100 * $done / $total)
                if ($percent -ne $lastShown) {
                    $status = '{0:N1} of {1:N1} MB' -f ($done / 1MB), ($total / 1MB)
                    Write-Progress -Activity "Downloading dgVoodoo2 $DgVoodooVersion" -Status $status -PercentComplete $percent
                    if ($percent % 10 -eq 0) { Say ("  {0,3} %  {1}" -f $percent, $status) }
                    $lastShown = $percent
                }
            }
        }
    } finally {
        $out.Dispose(); $in.Dispose(); $response.Dispose()
        Write-Progress -Activity "Downloading dgVoodoo2 $DgVoodooVersion" -Completed
    }
}

function Install-DgVoodoo {
    $steps = 5
    Step 1 $steps 'Checking the game folder'
    Test-GameFolder

    Step 2 $steps 'Checking for an existing dgVoodoo2'
    $todo = @()
    foreach ($f in $DgVoodooFiles) {
        $target = Join-Path $GameDir $f.To
        if (-not (Test-Path -LiteralPath $target)) { $todo += $f; continue }
        if ($null -eq $f.Sha256) { Say "  $($f.To): already there, kept"; continue }
        if ((Hash $target) -eq $f.Sha256) { Say "  $($f.To): dgVoodoo2 $DgVoodooVersion already there"; continue }
        Fail "$($f.To) in the game folder is not dgVoodoo2 $DgVoodooVersion (another version or another program). Nothing was changed. Remove or rename it yourself if you want this setup to install dgVoodoo2 $DgVoodooVersion."
    }
    if ($todo.Count -eq 0) {
        Say ''
        Say "dgVoodoo2 $DgVoodooVersion is already installed - nothing to do." 'Green'
        return
    }

    Step 3 $steps "Downloading dgVoodoo2 $DgVoodooVersion from its official GitHub release"
    Say "  $DgVoodooUrl"
    $temp = Join-Path ([IO.Path]::GetTempPath()) ('RivellonRenewed-setup-' + [Guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $temp | Out-Null
    try {
        $zip = Join-Path $temp 'dgVoodoo2.zip'
        try { Get-WithProgress $DgVoodooUrl $zip } catch { Fail "Download failed: $($_.Exception.Message). Check the internet connection and try again." }

        Step 4 $steps 'Checking the download (SHA-256)'
        $actual = Hash $zip
        if ($actual -ne $DgVoodooZipSha256) {
            Fail "The downloaded file is not the expected dgVoodoo2 $DgVoodooVersion (SHA-256 $actual, expected $DgVoodooZipSha256). Nothing was installed."
        }
        Say "  OK ($actual)" 'Green'

        Step 5 $steps 'Copying the dgVoodoo2 files into the game folder'
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive = [IO.Compression.ZipFile]::OpenRead($zip)
        $installed = @()
        try {
            foreach ($f in $todo) {
                $entry = $archive.Entries | Where-Object { $_.FullName -eq $f.From }
                if (@($entry).Count -ne 1) { Fail "$($f.From) not found in the dgVoodoo2 archive. Nothing more was installed." }
                $target = Join-Path $GameDir $f.To
                $part = "$target.part"
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $part, $true)
                if ($null -ne $f.Sha256 -and (Hash $part) -ne $f.Sha256) {
                    Remove-Item -LiteralPath $part
                    Fail "$($f.From) in the archive has an unexpected SHA-256. Nothing more was installed."
                }
                Move-Item -LiteralPath $part -Destination $target
                $installed += $f.To
                Say "  $($f.To)"
            }
        } finally {
            $archive.Dispose()
            if ($installed.Count -gt 0) { Add-Content -LiteralPath $Marker -Value $installed -Encoding UTF8 }
        }
    } finally {
        Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue
    }
    Say ''
    Say "dgVoodoo2 $DgVoodooVersion installed. Start the game as usual (through Steam or div.exe)." 'Green'
    Say 'Rivellon Renewed sets borderless full screen, no watermark and aspect-correct scaling at every start ([DgVoodoo] in RivellonRenewed.ini).'
}

function Remove-IfThere([string]$name, [string]$why = '') {
    $path = Join-Path $GameDir $name
    if (-not (Test-Path -LiteralPath $path)) { return }
    Remove-Item -LiteralPath $path -Recurse -Force
    Say "  removed $name$why"
}

function Uninstall-Mod {
    $steps = 3
    Step 1 $steps 'Checking the game folder'
    Test-GameFolder

    Step 2 $steps 'Removing dgVoodoo2 (only the files this setup installed)'
    if (Test-Path -LiteralPath $Marker) {
        foreach ($name in (Get-Content -LiteralPath $Marker -Encoding UTF8 | Where-Object { $_ } | Select-Object -Unique)) {
            $f = $DgVoodooFiles | Where-Object { $_.To -eq $name }
            $path = Join-Path $GameDir $name
            if (-not $f -or -not (Test-Path -LiteralPath $path)) { continue }
            if ($null -ne $f.Sha256 -and (Hash $path) -ne $f.Sha256) { Say "  kept $name (changed since the setup installed it)" 'Yellow'; continue }
            Remove-IfThere $name
        }
    } else {
        Say '  none installed by this setup'
    }

    Step 3 $steps 'Removing Rivellon Renewed'
    $loader = Join-Path $GameDir 'winmm.dll'
    if ((Test-Path -LiteralPath $loader) -and (Hash $loader) -eq $AsiLoaderSha256) {
        Remove-IfThere 'winmm.dll' ' (Ultimate ASI Loader)'
    } elseif (Test-Path -LiteralPath $loader) {
        Say '  kept winmm.dll (not the ASI loader that came with Rivellon Renewed)' 'Yellow'
    }
    foreach ($name in 'RivellonRenewed.asi', 'RivellonRenewed.ini', 'RivellonRenewed.log', 'RivellonRenewed.prev.log') {
        Remove-IfThere $name
    }
    # Last: this folder holds the running script itself (PowerShell has read it already).
    Remove-IfThere 'RivellonRenewed'
    Say ''
    Say 'Rivellon Renewed is uninstalled; the game runs as the original again. config.div was never changed.' 'Green'
    Say 'RivellonRenewed-Setup.cmd removes itself when this window closes.'
}

$code = 0
try {
    Say "Rivellon Renewed setup" 'White'
    if (-not $Install -and -not $Uninstall) {
        Say ''
        Say '  1  Install dgVoodoo2 (recommended on Windows: borderless full screen, sharp scaling)'
        Say '  2  Uninstall Rivellon Renewed (and the dgVoodoo2 files this setup installed)'
        Say '  Q  Quit'
        do { $choice = (Read-Host 'Choice').Trim().ToUpperInvariant() } until ($choice -in '1', '2', 'Q')
        $Install = $choice -eq '1'
        $Uninstall = $choice -eq '2'
    }
    if ($Install) { Install-DgVoodoo }
    elseif ($Uninstall) { Uninstall-Mod; $code = 10 }
} catch {
    Say ''
    Say "ERROR: $($_.Exception.Message)" 'Red'
    $code = 1
}
if (-not $NoPause) { Say ''; Read-Host 'Press Enter to close' | Out-Null }
exit $code

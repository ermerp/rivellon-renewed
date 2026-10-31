# Builds a Rivellon Renewed release zip from a committed state of the repository.
#
#   .\tools\release\New-Release.ps1 [-Ref HEAD] [-Suffix rc1] [-OutDir build\dist]
#
# 1. Exports the commit -Ref with git archive (only committed files - uncommitted changes are
#    never in a release) and builds it there (x86 Release, unit tests must pass).
# 2. Fetches Ultimate ASI Loader from its official GitHub release and its license from the
#    repository at the release commit; every download is checked against a pinned SHA-256, the
#    loader DLL against $AsiLoaderSha256 in setup/setup.ps1 (the uninstaller recognizes it by it).
# 3. Packs RivellonRenewed-<version>[-<suffix>].zip (layout: docs/dev/workflow.md, "Release zip")
#    and writes its SHA-256 (<zip>.sha256) and the commit it was built from (<zip>.commit)
#    next to it; publishing: Publish-Release.ps1.
# Windows PowerShell 5.1; needs git, cmake and the MSVC build tools (docs/dev/workflow.md).
[CmdletBinding()]
param(
    [string] $Ref = 'HEAD',
    [string] $Suffix = '',          # e.g. rc1 for a release candidate
    [string] $OutDir = (Join-Path $PSScriptRoot '..\..\build\dist')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2

# Ultimate ASI Loader (MIT, by ThirteenAG), x86 build without PDB; SHA-256 = GitHub digest.
$LoaderVersion = '9.7.4'
$LoaderZipUrl = 'https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/v9.7.4/Ultimate-ASI-Loader-NoPDB.zip'
$LoaderZipSha256 = '14B3A1AD018899571AC9AA01482977F3C6D49E6CBA99F552D01C5ACACD1315E1'
# The release zip has no license file; taken from the commit the tag v9.7.4 points to.
$LoaderLicenseUrl = 'https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/6b440669144c4a0bef5718ab155df160d231cd42/license'
$LoaderLicenseSha256 = 'CECA73C504E39084B0B80D2A437AEA9988313D1915418D3C629C3E4A8313F5A1'

$Stem = 'RivellonRenewed'

function Say([string]$text, [string]$color = 'Gray') { Write-Host $text -ForegroundColor $color }
function Hash([string]$path) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
function Invoke-Tool([string]$exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe $($arguments -join ' ') failed (exit code $LASTEXITCODE)" }
}
function Get-Checked([string]$url, [string]$file, [string]$sha256) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -Uri $url -OutFile $file -UseBasicParsing -UserAgent 'RivellonRenewed-Release'
    $actual = Hash $file
    if ($actual -ne $sha256) { throw "$url has SHA-256 $actual, expected $sha256" }
    Say "  $(Split-Path -Leaf $file): SHA-256 ok"
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$commit = (& git -C $repo rev-parse --verify "$Ref^{commit}").Trim()
if ($LASTEXITCODE -ne 0) { throw "Unknown ref: $Ref" }
if (& git -C $repo status --porcelain) { Say "Uncommitted changes in $repo are not part of the release (built from $Ref)." 'Yellow' }
$commitTime = [DateTimeOffset]::Parse((& git -C $repo log -1 --format=%cI $commit).Trim())

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
$work = Join-Path $OutDir 'work'
if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
$src = Join-Path $work 'src'
$downloads = Join-Path $work 'downloads'
New-Item -ItemType Directory -Force -Path $src, $downloads | Out-Null

Say "[1/4] Exporting $commit" 'Cyan'
$archive = Join-Path $work 'src.zip'
Invoke-Tool git @('-C', $repo, 'archive', '--format=zip', '-o', $archive, $commit)
Expand-Archive -LiteralPath $archive -DestinationPath $src
$cmakeLists = Get-Content -LiteralPath (Join-Path $src 'CMakeLists.txt') -Raw
if ($cmakeLists -notmatch 'set\(MOD_VERSION "([^"]+)"\)') { throw 'MOD_VERSION not found in CMakeLists.txt' }
$version = $Matches[1]
if ($cmakeLists -notmatch "set\(MOD_FILE_STEM `"$Stem`"\)") { throw "MOD_FILE_STEM is not $Stem" }
$setupScript = Get-Content -LiteralPath (Join-Path $src 'setup\setup.ps1') -Raw
if ($setupScript -notmatch "\`$AsiLoaderSha256 = '([0-9A-F]{64})'") { throw '$AsiLoaderSha256 not found in setup/setup.ps1' }
$loaderDllSha256 = $Matches[1]
$name = "$Stem-$version" + $(if ($Suffix) { "-$Suffix" } else { '' })
Say "  version $version, release $name"

Say '[2/4] Building (x86 Release) and running the unit tests' 'Cyan'
$build = Join-Path $src 'build'
Invoke-Tool cmake @('-S', $src, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'Win32')
Invoke-Tool cmake @('--build', $build, '--config', 'Release')
Invoke-Tool ctest @('-C', 'Release', '--test-dir', $build, '--output-on-failure')

Say "[3/4] Fetching Ultimate ASI Loader $LoaderVersion" 'Cyan'
$loaderZip = Join-Path $downloads 'Ultimate-ASI-Loader-NoPDB.zip'
Get-Checked $LoaderZipUrl $loaderZip $LoaderZipSha256
$loaderLicense = Join-Path $downloads 'Ultimate-ASI-Loader-license.txt'
Get-Checked $LoaderLicenseUrl $loaderLicense $LoaderLicenseSha256
Expand-Archive -LiteralPath $loaderZip -DestinationPath (Join-Path $downloads 'loader')
$loaderDll = Join-Path $downloads 'loader\dinput8.dll'
if ((Hash $loaderDll) -ne $loaderDllSha256) { throw "dinput8.dll is not the loader setup.ps1 expects (`$AsiLoaderSha256 $loaderDllSha256)" }

Say "[4/4] Packing $name.zip" 'Cyan'
# Path in the zip (= in the game folder) -> source file.
$entries = [ordered]@{
    'winmm.dll'                                   = $loaderDll
    "$Stem.asi"                                   = Join-Path $build "Release\$Stem.asi"
    "$Stem.ini"                                   = Join-Path $src "config\$Stem.ini"
    "$Stem-Setup.cmd"                             = Join-Path $src "setup\$Stem-Setup.cmd"
    "$Stem/setup.ps1"                             = Join-Path $src 'setup\setup.ps1'
    "$Stem/licenses/$Stem.txt"                    = Join-Path $src 'LICENSE'
    "$Stem/licenses/Ultimate-ASI-Loader.txt"      = $loaderLicense
    # Linked into RivellonRenewed.asi; BSD-2 asks for its notice with every binary. From the pinned
    # FetchContent checkout (CMakeLists.txt). stb_truetype is used under its public-domain option.
    "$Stem/licenses/MinHook.txt"                  = Join-Path $build '_deps\minhook-src\LICENSE.txt'
}
foreach ($font in Get-ChildItem -LiteralPath (Join-Path $src 'assets\fonts') -File | Sort-Object Name) {
    $entries["$Stem/fonts/$($font.Name)"] = $font.FullName
}
$zip = Join-Path $OutDir "$name.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip }
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$archiveOut = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($path in $entries.Keys) {
        $file = $entries[$path]
        if (-not (Test-Path -LiteralPath $file)) { throw "Missing: $file" }
        $entry = $archiveOut.CreateEntry($path, [IO.Compression.CompressionLevel]::Optimal)
        $entry.LastWriteTime = $commitTime  # same timestamps for every build of a commit
        $out = $entry.Open()
        try { $in = [IO.File]::OpenRead($file); try { $in.CopyTo($out) } finally { $in.Dispose() } } finally { $out.Dispose() }
    }
} finally { $archiveOut.Dispose() }

# Read the zip back: exactly the planned entries, each identical to its source.
$check = [IO.Compression.ZipFile]::OpenRead($zip)
try {
    $names = @($check.Entries | ForEach-Object { $_.FullName })
    if (($names -join '|') -ne (@($entries.Keys) -join '|')) { throw "Zip entries differ from the plan: $($names -join ', ')" }
    $sha = [Security.Cryptography.SHA256]::Create()
    foreach ($e in $check.Entries) {
        $s = $e.Open()
        try { $packed = [BitConverter]::ToString($sha.ComputeHash($s)).Replace('-', '') } finally { $s.Dispose() }
        if ($packed -ne (Hash $entries[$e.FullName])) { throw "$($e.FullName) in the zip differs from its source" }
    }
} finally { $check.Dispose() }

$zipSha256 = Hash $zip
[IO.File]::WriteAllText("$zip.sha256", "$($zipSha256.ToLowerInvariant())  $name.zip`n")
[IO.File]::WriteAllText("$zip.commit", "$commit`n")  # Publish-Release.ps1 tags this commit
Remove-Item -LiteralPath $work -Recurse -Force
Say ''
Say "$zip" 'Green'
Say "  commit  $commit"
Say "  SHA-256 $zipSha256"
foreach ($path in $entries.Keys) { Say "  $path" }

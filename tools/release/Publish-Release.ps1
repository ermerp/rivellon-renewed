# Publishes a release zip built by New-Release.ps1: tag, push of the tag, GitHub release.
#
#   .\tools\release\Publish-Release.ps1 -Version 0.02 [-NotesFile notes.md] [-DryRun]
#
# 1. Checks build\dist\RivellonRenewed-<version>.zip against its .sha256 and reads the commit it
#    was built from (<zip>.commit); MOD_VERSION at that commit must be -Version, the tag v<version>
#    must not exist yet (locally or on origin).
# 2. Release notes: the section "## v<version> – <date>" of CHANGELOG.md at that commit (or
#    -NotesFile), relative links made absolute (blob/v<version>/...), followed by the zip's SHA-256.
# 3. Shows commit, hash and notes and asks; only on "y": annotated tag v<version> on the commit,
#    git push origin v<version>, gh release create with the zip and its .sha256.
# -DryRun stops after step 3's overview (no gh needed). Needs git and the GitHub CLI (gh, logged in).
[CmdletBinding()]
param(
    [Parameter(Mandatory)] [string] $Version,
    [string] $NotesFile = '',
    [string] $DistDir = (Join-Path $PSScriptRoot '..\..\build\dist'),
    [switch] $DryRun
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version 2

$Stem = 'RivellonRenewed'

function Say([string]$text, [string]$color = 'Gray') { Write-Host $text -ForegroundColor $color }
function Invoke-Tool([string]$exe, [string[]]$arguments) {
    & $exe @arguments
    if ($LASTEXITCODE -ne 0) { throw "$exe $($arguments -join ' ') failed (exit code $LASTEXITCODE)" }
}

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

Say "[1/3] Checking the zip" 'Cyan'
$name = "$Stem-$Version"
$zip = Join-Path (Resolve-Path $DistDir).Path "$name.zip"
foreach ($f in $zip, "$zip.sha256", "$zip.commit") {
    if (-not (Test-Path -LiteralPath $f)) { throw "Missing: $f (build it with New-Release.ps1)" }
}
$zipSha256 = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
$recorded = ((Get-Content -LiteralPath "$zip.sha256" -Raw) -split '\s+')[0]
if ($zipSha256 -ne $recorded) { throw "$name.zip has SHA-256 $zipSha256, its .sha256 says $recorded" }
$commit = (Get-Content -LiteralPath "$zip.commit" -Raw).Trim()
& git -C $repo cat-file -e "$commit^{commit}"
if ($LASTEXITCODE -ne 0) { throw "Commit $commit from $name.zip.commit is not in the repository" }
$cmakeLists = (& git -C $repo show "${commit}:CMakeLists.txt") -join "`n"
if ($cmakeLists -notmatch 'set\(MOD_VERSION "([^"]+)"\)' -or $Matches[1] -ne $Version) { throw "MOD_VERSION at $commit is not $Version" }
$tag = "v$Version"
if (& git -C $repo tag -l $tag) { throw "Tag $tag exists locally" }
if (& git -C $repo ls-remote --tags origin "refs/tags/$tag") { throw "Tag $tag exists on origin" }

Say "[2/3] Release notes" 'Cyan'
if ($NotesFile) {
    $notes = (Get-Content -LiteralPath $NotesFile -Raw -Encoding UTF8).TrimEnd()
} else {
    $changelog = (& git -C $repo show "${commit}:CHANGELOG.md") -join "`n"
    $pattern = '(?ms)^## ' + [regex]::Escape($tag) + ' .*?\r?\n(.*?)(?=^## |\z)'
    if ($changelog -notmatch $pattern) { throw "No section '## $tag ...' in CHANGELOG.md at $commit" }
    $notes = $Matches[1].Trim()
    if (-not $notes) { throw "Section '## $tag' in CHANGELOG.md is empty" }
}
# Repository-relative links do not resolve on the release page: point them at the tagged files.
$remote = (& git -C $repo remote get-url origin).Trim() -replace '\.git$', ''
$notes = $notes -replace '\]\((?!https?://|#|mailto:)([^)]+)\)', "]($remote/blob/$tag/`$1)"
$notes += "`n`n**SHA-256** of ``$name.zip``: ``$zipSha256``"

Say "[3/3] Overview" 'Cyan'
Say "  tag     $tag (annotated) on $commit"
Say "  $(& git -C $repo log -1 --format='%h %s' $commit)"
Say "  zip     $zip"
Say "  SHA-256 $zipSha256"
Say '  --- notes ---'
Say $notes 'White'
Say '  -------------'
if ($DryRun) { Say 'Dry run - nothing published.' 'Yellow'; return }
$gh = Get-Command gh -ErrorAction SilentlyContinue
$gh = if ($gh) { $gh.Source } else { Join-Path $env:ProgramFiles 'GitHub CLI\gh.exe' }
if (-not (Test-Path -LiteralPath $gh)) { throw 'GitHub CLI (gh) not found - docs/dev/workflow.md' }
& $gh auth status 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'gh is not logged in - run: gh auth login' }
if ((Read-Host "Publish $name? (y/N)") -ne 'y') { Say 'Cancelled - nothing published.' 'Yellow'; return }

$notesOut = Join-Path ([IO.Path]::GetTempPath()) "$name-notes.md"
[IO.File]::WriteAllText($notesOut, $notes, (New-Object Text.UTF8Encoding $false))
try {
    Invoke-Tool git @('-C', $repo, 'tag', '-a', $tag, $commit, '-m', "Rivellon Renewed $tag")
    Invoke-Tool git @('-C', $repo, 'push', 'origin', $tag)
    Push-Location $repo
    try {
        Invoke-Tool $gh @('release', 'create', $tag, $zip, "$zip.sha256", '--verify-tag',
                          '--title', "Rivellon Renewed $tag", '--notes-file', $notesOut)
    } finally { Pop-Location }
} finally { Remove-Item -LiteralPath $notesOut -ErrorAction SilentlyContinue }
Say ''
Say "Published $tag" 'Green'

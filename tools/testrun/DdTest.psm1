# DdTest.psm1 - helpers for preparing and running Divine Divinity working copies.
# Windows PowerShell 5.1 compatible.

$script:SteamGameDir = 'C:\Program Files (x86)\Steam\steamapps\common\divine_divinity'

function Assert-WorkingCopy {
    # Refuses to touch the original Steam installation (it is read-only by project rule).
    param([Parameter(Mandatory)] [string] $GameDir)
    $full = (Resolve-Path -LiteralPath $GameDir).Path.TrimEnd('\')
    if ($full -ieq $script:SteamGameDir) {
        throw "Refusing to modify the original Steam installation: $full"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $full 'div.exe'))) {
        throw "No div.exe in $full"
    }
    return $full
}

function Set-IniValue {
    # Sets "Key = Value" inside [Section] of a dgVoodoo-style INI file, keeping comments and layout.
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Section,
        [Parameter(Mandatory)] [string] $Key,
        [Parameter(Mandatory)] [string] $Value
    )
    $lines = [System.Collections.Generic.List[string]](Get-Content -LiteralPath $Path)
    $inSection = $false
    for ($i = 0; $i -lt $lines.Count; $i++) {
        $line = $lines[$i]
        if ($line -match '^\s*\[(.+)\]\s*$') { $inSection = ($Matches[1] -ieq $Section); continue }
        if ($inSection -and $line -match "^\s*$([regex]::Escape($Key))\s*=") {
            $lines[$i] = '{0,-36} = {1}' -f $Key, $Value
            Set-Content -LiteralPath $Path -Value $lines -Encoding ASCII
            return
        }
    }
    throw "Key [$Section] $Key not found in $Path"
}

function Set-DgVoodooProfile {
    # Applies the dgVoodoo2 settings from PLAN.md Phase 0, step 6 to <GameDir>\dgVoodoo.conf.
    param(
        [Parameter(Mandatory)] [string] $GameDir,
        [int] $Adapter = 1,          # DXGI adapter ordinal of the real GPU
        [switch] $Watermark          # show the dgVoodoo watermark (proves dgVoodoo is active)
    )
    $dir = Assert-WorkingCopy $GameDir
    $conf = Join-Path $dir 'dgVoodoo.conf'
    $settings = @(
        # General tab: D3D11 FL 11.0, real GPU, full screen, all checkboxes off except color profile
        @('General', 'OutputAPI', 'd3d11_fl11_0'),
        @('General', 'Adapters', "$Adapter"),
        @('General', 'FullScreenOutput', 'default'),
        @('General', 'FullScreenMode', 'true'),
        @('General', 'ScalingMode', 'unspecified'),
        @('General', 'ProgressiveScanlineOrder', 'false'),
        @('General', 'EnumerateRefreshRates', 'false'),
        @('General', 'InheritColorProfileInFullScreenMode', 'true'),
        @('General', 'KeepWindowAspectRatio', 'false'),
        @('General', 'CaptureMouse', 'false'),
        @('General', 'CenterAppWindow', 'false'),
        @('General', 'DisableScreenSaver', 'false'),
        # DirectX tab: no passthru, virtual 3D card, 128 MB, app-driven quality, only vSync forced
        @('DirectX', 'DisableAndPassThru', 'false'),
        @('DirectX', 'VideoCard', 'internal3D'),
        @('DirectX', 'VRAM', '128'),
        @('DirectX', 'Filtering', 'appdriven'),
        @('DirectX', 'Mipmapping', 'appdriven'),
        @('DirectX', 'KeepFilterIfPointSampled', 'false'),
        @('DirectX', 'Resolution', 'unforced'),
        @('DirectX', 'Antialiasing', 'appdriven'),
        @('DirectX', 'AppControlledScreenMode', 'false'),
        @('DirectX', 'DisableAltEnterToToggleScreenMode', 'false'),
        @('DirectX', 'Bilinear2DOperations', 'false'),
        @('DirectX', 'PhongShadingWhenPossible', 'false'),
        @('DirectX', 'ForceVerticalSync', 'true'),
        @('DirectX', 'dgVoodooWatermark', $(if ($Watermark) { 'true' } else { 'false' })),
        @('DirectX', 'FastVideoMemoryAccess', 'false')
    )
    foreach ($s in $settings) { Set-IniValue -Path $conf -Section $s[0] -Key $s[1] -Value $s[2] }
}

function Set-DivConfigValue {
    # Sets "key value" in config.div (the game's plain-text config); appends the key with a comment if missing.
    param(
        [Parameter(Mandatory)] [string] $Path,
        [Parameter(Mandatory)] [string] $Key,
        [Parameter(Mandatory)] [string] $Value,
        [string] $Comment
    )
    $lines = [System.Collections.Generic.List[string]](Get-Content -LiteralPath $Path)
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match "^\s*$([regex]::Escape($Key))\s+\S") {
            $lines[$i] = "$Key $Value"
            Set-Content -LiteralPath $Path -Value $lines -Encoding ASCII
            return
        }
    }
    if ($Comment) { $lines.Add(''); $lines.Add("// $Comment") }
    $lines.Add("$Key $Value")
    Set-Content -LiteralPath $Path -Value $lines -Encoding ASCII
}

function Set-DivConfig {
    # Applies the game settings from PLAN.md Phase 0, step 6: resolution, smoothness on, alpha bit off.
    # The renderer itself is selected in slashed.cfg (first DWORD: 0 = Direct3D 6, 1 = Glide,
    # 2 = DirectDraw, 3 = Software).
    param(
        [Parameter(Mandatory)] [string] $GameDir,
        [int] $Width = 1920,
        [int] $Height = 1080
    )
    $dir = Assert-WorkingCopy $GameDir
    $cfg = Join-Path $dir 'config.div'
    Set-DivConfigValue -Path $cfg -Key 'width' -Value "$Width" -Comment 'display width'
    Set-DivConfigValue -Path $cfg -Key 'height' -Value "$Height" -Comment 'display height'
    Set-DivConfigValue -Path $cfg -Key 'smooth' -Value '1' -Comment 'activate smooth mode'
    Set-DivConfigValue -Path $cfg -Key 'alpha bit' -Value '0' -Comment 'Select alpha bit'
    [System.IO.File]::WriteAllBytes((Join-Path $dir 'slashed.cfg'), [byte[]](0, 0, 0, 0))
}

Export-ModuleMember -Function Assert-WorkingCopy, Set-IniValue, Set-DgVoodooProfile, Set-DivConfigValue, Set-DivConfig

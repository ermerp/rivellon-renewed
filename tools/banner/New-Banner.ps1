# Renders the banner: "RIVELLON / RENEWED" in the bundled Uncial Antiqua (OFL), in the main menu's
# capital color with a shine across the strokes, black edge and shadow, on dark stone (or
# transparent). Our own drawing, no game content. Windows PowerShell 5.1 (System.Drawing).
#
#   tools\banner\New-Banner.ps1 [-Out <png>] [-Height <px>] [-Transparent]
#   social preview: -Height 640 (1280x640, GitHub's size); README: a flatter height
param([string]$Out = (Join-Path $PSScriptRoot '..\..\docs\images\banner.png'), [int]$Height = 640, [switch]$Transparent)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$W = 1280; $H = $Height
$dy = ($H - 640) / 2  # the text is laid out for 640 lines and kept centered
$fonts = New-Object System.Drawing.Text.PrivateFontCollection
$fonts.AddFontFile((Join-Path $PSScriptRoot '..\..\assets\fonts\UncialAntiqua-Regular.ttf'))
$family = $fonts.Families[0]
function C([string]$hex) { [System.Drawing.ColorTranslator]::FromHtml($hex) }

$bmp = New-Object System.Drawing.Bitmap $W, $H
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = 'AntiAlias'
$rect = New-Object System.Drawing.Rectangle 0, 0, $W, $H

# Background: dark stone with fine grain and a vignette.
if (-not $Transparent) {
    $g.FillRectangle((New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, (C '#1E1A16'), (C '#0A0908'), 90), $rect)
    $rnd = New-Object Random 7
    for ($i = 0; $i -lt ($W * $H / 18); $i++) {
        $v = $rnd.Next(0, 255)
        $grain = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(22, $v, $v, $v))
        $g.FillRectangle($grain, $rnd.Next(0, $W), $rnd.Next(0, $H), 2, 2)
        $grain.Dispose()
    }
    $ellipse = New-Object System.Drawing.Drawing2D.GraphicsPath
    $ellipse.AddEllipse(-$W * 0.25, -$H * 0.35, $W * 1.5, $H * 1.7)
    $vignette = New-Object System.Drawing.Drawing2D.PathGradientBrush $ellipse
    $vignette.CenterColor = [System.Drawing.Color]::FromArgb(0, 0, 0, 0)
    $vignette.SurroundColors = @([System.Drawing.Color]::FromArgb(150, 0, 0, 0))
    $g.FillRectangle($vignette, $rect)
}

# Lines: text, size in pixels, vertical center.
$format = New-Object System.Drawing.StringFormat
$format.Alignment = 'Center'; $format.LineAlignment = 'Center'
foreach ($line in @(@('RIVELLON', 170, 260), @('RENEWED', 104, 430))) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddString($line[0], $family, 0, [single]$line[1], (New-Object System.Drawing.PointF ($W / 2), ($line[2] + $dy)), $format)
    $shadow = $path.Clone()
    $offset = New-Object System.Drawing.Drawing2D.Matrix; $offset.Translate(5, 6); $shadow.Transform($offset)
    $g.FillPath((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(170, 0, 0, 0))), $shadow)
    $edge = New-Object System.Drawing.Pen (C '#000000'), 7
    $edge.LineJoin = 'Round'
    $g.DrawPath($edge, $path)
    # Menu capital color (mean #C2B996, docs/re/glyph-layout.md), lighter across the middle.
    $bounds = $path.GetBounds()
    $box = New-Object System.Drawing.RectangleF $bounds.X, ($bounds.Y - 1), $bounds.Width, ($bounds.Height + 2)
    $fill = New-Object System.Drawing.Drawing2D.LinearGradientBrush $box, (C '#8F8770'), (C '#8A8064'), 90
    $blend = New-Object System.Drawing.Drawing2D.ColorBlend 3
    $blend.Colors = @((C '#8F8770'), (C '#E4DCBC'), (C '#8A8064')); $blend.Positions = @(0.0, 0.45, 1.0)
    $fill.InterpolationColors = $blend
    $g.FillPath($fill, $path)
}

$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "Banner: $Out"

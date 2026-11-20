# Renders the RPlay K&M logo (same shapes as assets/logo.svg) into assets/rplaykm.ico.
# Run from the repo root: powershell -File tools/make_icon.ps1
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'

function New-LogoPath {
    # Glyph outlines in the SVG's 120x120 space, already shifted by translate(-2 -6).
    $p = New-Object Drawing.Drawing2D.GraphicsPath ([Drawing.Drawing2D.FillMode]::Alternate)
    function pt($x, $y) { New-Object Drawing.PointF ([float]($x - 2)), ([float]($y - 6)) }
    # R
    $p.StartFigure()
    $p.AddLine((pt 14 24), (pt 44 24)); $p.AddBezier((pt 44 24), (pt 57 24), (pt 65 32), (pt 65 44))
    $p.AddLine((pt 65 44), (pt 65 47)); $p.AddBezier((pt 65 47), (pt 65 55), (pt 61 60), (pt 55 63))
    $p.AddLine((pt 55 63), (pt 67 92)); $p.AddLine((pt 67 92), (pt 48 92)); $p.AddLine((pt 48 92), (pt 39 68))
    $p.AddLine((pt 39 68), (pt 33 68)); $p.AddLine((pt 33 68), (pt 33 92)); $p.AddLine((pt 33 92), (pt 14 92))
    $p.CloseFigure()
    $p.StartFigure()
    $p.AddLine((pt 33 40), (pt 42 40)); $p.AddBezier((pt 42 40), (pt 45.5 40), (pt 47 42), (pt 47 45))
    $p.AddLine((pt 47 45), (pt 47 47)); $p.AddBezier((pt 47 47), (pt 47 50), (pt 45.5 52), (pt 42 52))
    $p.AddLine((pt 42 52), (pt 33 52))
    $p.CloseFigure()
    # p
    $p.StartFigure()
    $p.AddLine((pt 72 46), (pt 96 46)); $p.AddBezier((pt 96 46), (pt 104.5 46), (pt 110 51.5), (pt 110 60))
    $p.AddLine((pt 110 60), (pt 110 78)); $p.AddBezier((pt 110 78), (pt 110 86.5), (pt 104.5 92), (pt 96 92))
    $p.AddLine((pt 96 92), (pt 88 92)); $p.AddLine((pt 88 92), (pt 88 108)); $p.AddLine((pt 88 108), (pt 72 108))
    $p.CloseFigure()
    $p.StartFigure()
    $p.AddLine((pt 88 60), (pt 91.5 60)); $p.AddBezier((pt 91.5 60), (pt 93 60), (pt 94 61), (pt 94 62.5))
    $p.AddLine((pt 94 62.5), (pt 94 75.5)); $p.AddBezier((pt 94 75.5), (pt 94 77), (pt 93 78), (pt 91.5 78))
    $p.AddLine((pt 91.5 78), (pt 88 78))
    $p.CloseFigure()
    $p
}

function Render-Logo([int]$size) {
    $bmp = New-Object Drawing.Bitmap $size, $size, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'; $g.PixelOffsetMode = 'HighQuality'
    $g.ScaleTransform($size / 120.0, $size / 120.0)
    $tile = New-Object Drawing.Drawing2D.GraphicsPath
    $r = 26; $x = 4; $y = 4; $w = 112
    $tile.AddArc($x, $y, 2 * $r, 2 * $r, 180, 90); $tile.AddArc($x + $w - 2 * $r, $y, 2 * $r, 2 * $r, 270, 90)
    $tile.AddArc($x + $w - 2 * $r, $y + $w - 2 * $r, 2 * $r, 2 * $r, 0, 90); $tile.AddArc($x, $y + $w - 2 * $r, 2 * $r, 2 * $r, 90, 90)
    $tile.CloseFigure()
    $g.FillPath((New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(255, 0x1C, 0x0C, 0x3E))), $tile)
    $letters = New-LogoPath
    $pen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(255, 0xFF, 0x6F, 0xC8)), 3
    $pen.LineJoin = 'Round'
    $g.DrawPath($pen, $letters)
    $brush = New-Object Drawing.Drawing2D.LinearGradientBrush (New-Object Drawing.PointF 0, 14), (New-Object Drawing.PointF 0, 104), ([Drawing.Color]::White), ([Drawing.Color]::White)
    $blend = New-Object Drawing.Drawing2D.ColorBlend 4
    $blend.Colors = @([Drawing.Color]::FromArgb(255, 0x3F, 0xA6, 0xF0), [Drawing.Color]::FromArgb(255, 0x9A, 0x4D, 0xE6),
                      [Drawing.Color]::FromArgb(255, 0xFF, 0x6F, 0xC8), [Drawing.Color]::FromArgb(255, 0xFF, 0xA2, 0xB8))
    $blend.Positions = @([float]0, [float]0.45, [float]0.85, [float]1)
    $brush.InterpolationColors = $blend
    $g.FillPath($brush, $letters)
    $g.Dispose()
    $bmp
}

$sizes = 256, 64, 48, 32, 24, 16
$images = foreach ($s in $sizes) {
    $ms = New-Object IO.MemoryStream
    (Render-Logo $s).Save($ms, [Drawing.Imaging.ImageFormat]::Png)
    , $ms.ToArray()
}
$out = New-Object IO.MemoryStream
$bw = New-Object IO.BinaryWriter $out
$bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]; $d = $images[$i]
    $bw.Write([byte]($s % 256)); $bw.Write([byte]($s % 256)); $bw.Write([byte]0); $bw.Write([byte]0)
    $bw.Write([uint16]1); $bw.Write([uint16]32); $bw.Write([uint32]$d.Length); $bw.Write([uint32]$offset)
    $offset += $d.Length
}
foreach ($d in $images) { $bw.Write($d) }
[IO.File]::WriteAllBytes((Join-Path $PSScriptRoot '..\assets\rplaykm.ico'), $out.ToArray())
(Render-Logo 256).Save((Join-Path $PSScriptRoot '..\assets\logo.png'), [Drawing.Imaging.ImageFormat]::Png)
'ok'

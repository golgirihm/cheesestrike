# Draws CheeseStrike's splash screens (game and editor) and the Windows icon:
#   tools/art/brand.ps1 -SplashDir Content/Splash -IconPath Build/Windows/Application.ico -PreviewDir <somewhere>
param(
  [Parameter(Mandatory=$true)][string]$SplashDir,
  [Parameter(Mandatory=$true)][string]$IconPath,
  [Parameter(Mandatory=$true)][string]$PreviewDir
)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"

$cheese     = [System.Drawing.Color]::FromArgb(255, 246, 190, 58)
$cheeseDark = [System.Drawing.Color]::FromArgb(255, 214, 148, 28)
$rind       = [System.Drawing.Color]::FromArgb(255, 196, 120, 18)
$hole       = [System.Drawing.Color]::FromArgb(255, 200, 130, 22)
$ink        = [System.Drawing.Color]::FromArgb(255, 235, 230, 220)
$muted      = [System.Drawing.Color]::FromArgb(255, 150, 140, 125)

function P([float]$x, [float]$y) { return New-Object System.Drawing.PointF $x, $y }

function Draw-Wedge($g, [float]$x, [float]$y, [float]$s) {
  # A wedge of cheese seen from the front and a little above: the triangular
  # top, the long front face under it, and the rind on the back face, with
  # holes cut in the top and the front.
  $d = 0.32 * $s
  $tip  = P ($x + 0.04*$s) ($y + 0.56*$s)
  $back = P ($x + 0.60*$s) ($y + 0.08*$s)
  $side = P ($x + 0.96*$s) ($y + 0.34*$s)
  $down = { param($p) P $p.X ($p.Y + $d) }
  $rindFace = [System.Drawing.PointF[]]@($back, $side, (& $down $side), (& $down $back))
  $topFace = [System.Drawing.PointF[]]@($tip, $back, $side)
  $frontFace = [System.Drawing.PointF[]]@($tip, $side, (& $down $side), (& $down $tip))
  $g.FillPolygon((New-Object System.Drawing.SolidBrush $rind), $rindFace)
  $g.FillPolygon((New-Object System.Drawing.SolidBrush $cheeseDark), $frontFace)
  $g.FillPolygon((New-Object System.Drawing.SolidBrush $cheese), $topFace)
  $holeBrush = New-Object System.Drawing.SolidBrush $hole
  # On the top, holes are flattened ellipses; on the front, rounder.
  foreach ($h in @(@(0.55, 0.30, 0.13, 0.5), @(0.74, 0.30, 0.07, 0.5), @(0.40, 0.42, 0.07, 0.5), @(0.62, 0.18, 0.05, 0.5), @(0.20, 0.66, 0.09, 0.8), @(0.42, 0.72, 0.06, 0.8), @(0.62, 0.62, 0.11, 0.8), @(0.85, 0.56, 0.06, 0.8))) {
    $w = $h[2] * $s
    $g.FillEllipse($holeBrush, [float]($x + $h[0]*$s - $w/2), [float]($y + $h[1]*$s - $w*$h[3]/2), [float]$w, [float]($w*$h[3]))
  }
}

function New-Canvas([int]$w, [int]$h) {
  $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
  $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  return @($bmp, $g)
}

function Save-Splash([string]$path, [string]$preview, [bool]$editor) {
  $w = 720; $h = 370
  $bmp, $g = New-Canvas $w $h
  $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
  $bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, ([System.Drawing.Color]::FromArgb(255, 34, 28, 22)), ([System.Drawing.Color]::FromArgb(255, 14, 12, 10)), 90.0
  $g.FillRectangle($bg, $rect)
  # Faint holes across the background, like the lobby's tile.
  $faint = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(22, 246, 190, 58))
  $rand = New-Object System.Random 7
  for ($i = 0; $i -lt 40; $i++) {
    $d = 12 + $rand.Next(40)
    $g.FillEllipse($faint, [float]$rand.Next($w), [float]$rand.Next($h), [float]$d, [float]$d)
  }
  Draw-Wedge $g 40 70 220
  $title = New-Object System.Drawing.Font "Segoe UI Black", 44, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
  $sub = New-Object System.Drawing.Font "Segoe UI Semibold", 22, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
  $shadow = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(200, 0, 0, 0))
  $g.DrawString("CHEESESTRIKE", $title, $shadow, 293, 123)
  $g.DrawString("CHEESESTRIKE", $title, (New-Object System.Drawing.SolidBrush $cheese), 290, 120)
  $g.DrawString("GOUDA OVERLOAD", $sub, (New-Object System.Drawing.SolidBrush $ink), 294, 182)
  if (-not $editor) {
    $small = New-Object System.Drawing.Font "Segoe UI", 13, ([System.Drawing.FontStyle]::Regular), ([System.Drawing.GraphicsUnit]::Pixel)
    $g.DrawString("Loading the cheese...", $small, (New-Object System.Drawing.SolidBrush $muted), 296, 222)
  }
  $g.Dispose()
  # The splash is a 24-bit BMP, as the engine's own are.
  $flat = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $fg = [System.Drawing.Graphics]::FromImage($flat)
  $fg.DrawImage($bmp, 0, 0, $w, $h)
  $fg.Dispose()
  $flat.Save($path, [System.Drawing.Imaging.ImageFormat]::Bmp)
  $flat.Save($preview, [System.Drawing.Imaging.ImageFormat]::Png)
  $flat.Dispose(); $bmp.Dispose()
}

function Icon-Png([int]$size) {
  $bmp, $g = New-Canvas $size $size
  $g.Clear([System.Drawing.Color]::Transparent)
  # A rounded dark tile with the wedge on it.
  $r = [float]($size * 0.18)
  $path = New-Object System.Drawing.Drawing2D.GraphicsPath
  $s = [float]($size - 1)
  $path.AddArc(0, 0, 2*$r, 2*$r, 180, 90)
  $path.AddArc($s - 2*$r, 0, 2*$r, 2*$r, 270, 90)
  $path.AddArc($s - 2*$r, $s - 2*$r, 2*$r, 2*$r, 0, 90)
  $path.AddArc(0, $s - 2*$r, 2*$r, 2*$r, 90, 90)
  $path.CloseFigure()
  $g.FillPath((New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 30, 25, 20))), $path)
  Draw-Wedge $g ([float]($size*0.06)) ([float]($size*0.04)) ([float]($size*0.88))
  $g.Dispose()
  $ms = New-Object System.IO.MemoryStream
  $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
  $bmp.Dispose()
  return ,$ms.ToArray()
}

New-Item -ItemType Directory -Force $SplashDir | Out-Null
New-Item -ItemType Directory -Force $PreviewDir | Out-Null
Save-Splash (Join-Path $SplashDir "Splash.bmp") (Join-Path $PreviewDir "splash.png") $false
Save-Splash (Join-Path $SplashDir "EdSplash.bmp") (Join-Path $PreviewDir "edsplash.png") $true

# The icon: one file with PNG images at each size Windows asks for.
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = @()
foreach ($size in $sizes) { $images += ,(Icon-Png $size) }
[System.IO.File]::WriteAllBytes((Join-Path $PreviewDir "icon256.png"), $images[-1])
$out = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter $out
$writer.Write([UInt16]0); $writer.Write([UInt16]1); $writer.Write([UInt16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
  $dim = if ($sizes[$i] -ge 256) { 0 } else { $sizes[$i] }
  $writer.Write([byte]$dim); $writer.Write([byte]$dim); $writer.Write([byte]0); $writer.Write([byte]0)
  $writer.Write([UInt16]1); $writer.Write([UInt16]32)
  $writer.Write([UInt32]$images[$i].Length); $writer.Write([UInt32]$offset)
  $offset += $images[$i].Length
}
foreach ($img in $images) { $writer.Write($img) }
$writer.Flush()
New-Item -ItemType Directory -Force (Split-Path $IconPath) | Out-Null
[System.IO.File]::WriteAllBytes($IconPath, $out.ToArray())
"done"

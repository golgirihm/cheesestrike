# Draws a seamless 512x512 mask of Swiss-cheese holes: white in a hole,
# fading to black at its rim, black on the cheese. Holes that cross an edge
# are drawn again on the far side, so the tile repeats without a seam.
param([Parameter(Mandatory=$true)][string]$Out)
Add-Type -AssemblyName System.Drawing
$size = 512
$bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$g.Clear([System.Drawing.Color]::Black)
$rand = New-Object System.Random 11
$placed = @()
$tries = 0
while ($placed.Count -lt 26 -and $tries -lt 4000) {
  $tries++
  $r = 10 + [Math]::Pow($rand.NextDouble(), 1.8) * 46
  $x = $rand.NextDouble() * $size
  $y = $rand.NextDouble() * $size
  # Keep holes apart, measured across the wrap.
  $ok = $true
  foreach ($p in $placed) {
    $dx = [Math]::Abs($p[0] - $x); $dx = [Math]::Min($dx, $size - $dx)
    $dy = [Math]::Abs($p[1] - $y); $dy = [Math]::Min($dy, $size - $dy)
    if ([Math]::Sqrt($dx*$dx + $dy*$dy) -lt ($p[2] + $r + 14)) { $ok = $false; break }
  }
  if ($ok) { $placed += ,@($x, $y, $r) }
}
foreach ($p in $placed) {
  foreach ($ox in @(-$size, 0, $size)) {
    foreach ($oy in @(-$size, 0, $size)) {
      $cx = $p[0] + $ox; $cy = $p[1] + $oy; $r = $p[2]
      if ($cx + $r -lt 0 -or $cx - $r -gt $size -or $cy + $r -lt 0 -or $cy - $r -gt $size) { continue }
      $path = New-Object System.Drawing.Drawing2D.GraphicsPath
      $path.AddEllipse([float]($cx - $r), [float]($cy - $r), [float](2*$r), [float](2*$r))
      $brush = New-Object System.Drawing.Drawing2D.PathGradientBrush $path
      $brush.CenterColor = [System.Drawing.Color]::White
      $brush.SurroundColors = [System.Drawing.Color[]]@([System.Drawing.Color]::FromArgb(255, 40, 40, 40))
      # Mostly solid, darkening only at the rim.
      $blend = New-Object System.Drawing.Drawing2D.Blend 3
      $blend.Factors = [float[]]@(0.0, 0.85, 1.0)
      $blend.Positions = [float[]]@(0.0, 0.18, 1.0)
      $brush.Blend = $blend
      $g.FillPath($brush, $path)
    }
  }
}
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
"holes $($placed.Count)"

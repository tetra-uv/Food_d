# make_icon.ps1: development tool. Draws the program icon (a flat blue rounded square with the white letters "CB")
# with System.Drawing and writes res\app.ico with images of 16, 24, 32, 48, 64 and 256 pixels.
# The small images are stored as classic bitmaps (BGRA plus an empty mask), the 256 pixel image as PNG, which is how
# Windows icons are usually made. Nothing is installed or downloaded.
# Usage: powershell -ExecutionPolicy Bypass -File desktop-app\tools\make_icon.ps1 [-Out <file>]
param(
    [string]$Out = ""
)

Add-Type -AssemblyName System.Drawing

if ($Out -eq "")
{
    $Out = Join-Path (Split-Path -Parent $PSScriptRoot) "res\app.ico"
}

$sizes = @(16, 24, 32, 48, 64, 256)

# Draws one image of the icon: a rounded square in the blue of the page titles, with the letters CB in white.
function New-IconBitmap([int]$size)
{
    $bitmap = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bitmap)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
    $g.Clear([System.Drawing.Color]::Transparent)

    $margin = [math]::Max(0, [math]::Round($size * 0.04))
    $side = $size - 2 * $margin
    $radius = [math]::Max(2, [math]::Round($side * 0.22))
    $diameter = 2 * $radius
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $path.AddArc($margin, $margin, $diameter, $diameter, 180, 90)
    $path.AddArc($margin + $side - $diameter, $margin, $diameter, $diameter, 270, 90)
    $path.AddArc($margin + $side - $diameter, $margin + $side - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc($margin, $margin + $side - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()

    $blue = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 31, 78, 140))
    $g.FillPath($blue, $path)

    # small images get relatively larger letters, so CB stays readable at 16 pixels
    $ratio = if ($size -le 24) { 0.60 } elseif ($size -le 32) { 0.54 } else { 0.50 }
    $fontSize = [single]($size * $ratio)
    $font = New-Object System.Drawing.Font("Segoe UI", $fontSize, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    # typographic format: no extra padding around the text and no wrapping, so both letters always fit on one line
    $format = [System.Drawing.StringFormat]::GenericTypographic.Clone()
    $format.FormatFlags = $format.FormatFlags -bor [System.Drawing.StringFormatFlags]::NoWrap -bor [System.Drawing.StringFormatFlags]::NoClip
    $format.Alignment = [System.Drawing.StringAlignment]::Center
    $format.LineAlignment = [System.Drawing.StringAlignment]::Center
    $area = New-Object System.Drawing.RectangleF 0, ([single]($size * 0.02)), $size, $size
    $g.DrawString("CB", $font, [System.Drawing.Brushes]::White, $area, $format)

    $g.Dispose()
    return $bitmap
}

# The bytes of one small image in the classic icon format: a bitmap header, the pixels (bottom row first) and an empty mask.
function Get-DibBytes([System.Drawing.Bitmap]$bitmap)
{
    $size = $bitmap.Width
    $stream = New-Object System.IO.MemoryStream
    $writer = New-Object System.IO.BinaryWriter $stream
    $writer.Write([int]40)
    $writer.Write([int]$size)
    $writer.Write([int](2 * $size))
    $writer.Write([int16]1)
    $writer.Write([int16]32)
    $writer.Write([int]0)
    $writer.Write([int]($size * $size * 4))
    $writer.Write([int]0)
    $writer.Write([int]0)
    $writer.Write([int]0)
    $writer.Write([int]0)

    for ($y = $size - 1; $y -ge 0; $y--)
    {
        for ($x = 0; $x -lt $size; $x++)
        {
            $c = $bitmap.GetPixel($x, $y)
            $writer.Write([byte]$c.B)
            $writer.Write([byte]$c.G)
            $writer.Write([byte]$c.R)
            $writer.Write([byte]$c.A)
        }
    }

    $maskRow = [int]([math]::Ceiling($size / 32.0) * 4)
    $writer.Write((New-Object byte[] ($maskRow * $size)))
    $writer.Flush()
    return $stream.ToArray()
}

# The bytes of one image as a PNG file.
function Get-PngBytes([System.Drawing.Bitmap]$bitmap)
{
    $stream = New-Object System.IO.MemoryStream
    $bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    return $stream.ToArray()
}

$images = @()

foreach ($size in $sizes)
{
    $bitmap = New-IconBitmap $size

    if ($size -ge 256)
    {
        $images += , (Get-PngBytes $bitmap)
    }
    else
    {
        $images += , (Get-DibBytes $bitmap)
    }

    $bitmap.Dispose()
}

# the icon file: a header, one directory entry for each image, then the images
$temporary = $Out + ".tmp"
$file = New-Object System.IO.BinaryWriter ([System.IO.File]::Open($temporary, [System.IO.FileMode]::Create))
$file.Write([int16]0)
$file.Write([int16]1)
$file.Write([int16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count

for ($i = 0; $i -lt $sizes.Count; $i++)
{
    $size = $sizes[$i]
    $dimension = if ($size -ge 256) { 0 } else { $size }
    $file.Write([byte]$dimension)
    $file.Write([byte]$dimension)
    $file.Write([byte]0)
    $file.Write([byte]0)
    $file.Write([int16]1)
    $file.Write([int16]32)
    $file.Write([int]$images[$i].Length)
    $file.Write([int]$offset)
    $offset += $images[$i].Length
}

for ($i = 0; $i -lt $sizes.Count; $i++)
{
    # the functions hand their bytes back as a list of objects, so they are turned into a byte array here
    $file.Write([byte[]]$images[$i])
}

$file.Close()
Move-Item -LiteralPath $temporary -Destination $Out -Force
Write-Host ("Wrote " + $Out + " (" + (Get-Item $Out).Length + " bytes, " + $sizes.Count + " images)")

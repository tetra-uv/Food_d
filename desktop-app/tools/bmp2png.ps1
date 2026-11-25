# bmp2png.ps1: development tool. Converts every .bmp file of a folder to a .png file next to it
# (System.Drawing, nothing is installed). The test mode --screenshots writes the bmp files.
# Usage: powershell -ExecutionPolicy Bypass -File desktop-app\tools\bmp2png.ps1 -Folder <folder>
param(
    [Parameter(Mandatory = $true)]
    [string]$Folder
)

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path -LiteralPath $Folder))
{
    Write-Host "The folder does not exist: $Folder"
    exit 1
}

$count = 0

foreach ($file in Get-ChildItem -LiteralPath $Folder -Filter *.bmp)
{
    $target = [System.IO.Path]::ChangeExtension($file.FullName, ".png")
    $image = [System.Drawing.Image]::FromFile($file.FullName)

    try
    {
        $image.Save($target, [System.Drawing.Imaging.ImageFormat]::Png)
        $count++
    }
    finally
    {
        $image.Dispose()
    }
}

Write-Host "Converted $count file(s) in $Folder"
exit 0

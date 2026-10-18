param(
    [Parameter(Mandatory=$true)][string]$Source,
    [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$encoded = [System.IO.File]::ReadAllText($Source).Trim()
$sourceBytes = [Convert]::FromBase64String($encoded)
$sourceStream = New-Object System.IO.MemoryStream(,$sourceBytes)
$sourceImage = [System.Drawing.Image]::FromStream($sourceStream)
$sizes = @(16, 24, 32, 48, 64, 128, 256)
$entries = New-Object 'System.Collections.Generic.List[byte[]]'
try {
    foreach ($size in $sizes) {
        $bitmap = [System.Drawing.Bitmap]::new($size,$size,[System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.DrawImage($sourceImage,0,0,$size,$size)
            } finally { $graphics.Dispose() }
            $memory = New-Object System.IO.MemoryStream
            try {
                $bitmap.Save($memory,[System.Drawing.Imaging.ImageFormat]::Png)
                $entries.Add($memory.ToArray())
            } finally { $memory.Dispose() }
        } finally { $bitmap.Dispose() }
    }
    $destination = [System.IO.File]::Create($Output)
    try {
        $writer = New-Object System.IO.BinaryWriter($destination)
        $writer.Write([uint16]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]$sizes.Count)
        $offset = 6 + 16 * $sizes.Count
        for ($i=0; $i -lt $sizes.Count; $i++) {
            $size = $sizes[$i]
            $writer.Write([byte]($size % 256))
            $writer.Write([byte]($size % 256))
            $writer.Write([byte]0)
            $writer.Write([byte]0)
            $writer.Write([uint16]1)
            $writer.Write([uint16]32)
            $writer.Write([uint32]$entries[$i].Length)
            $writer.Write([uint32]$offset)
            $offset += $entries[$i].Length
        }
        foreach ($data in $entries) { $writer.Write($data) }
        $writer.Flush()
    } finally { $destination.Dispose() }
} finally {
    $sourceImage.Dispose()
    $sourceStream.Dispose()
}

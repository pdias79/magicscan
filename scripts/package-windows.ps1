param(
    [string]$BuildDir = "build",
    [string]$OutputDir = "dist",
    [string]$Version = ""
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $Version) {
    $cmake = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
    if ($cmake -notmatch 'project\(magicscan VERSION (\d+\.\d+(?:\.\d+)?)') {
        throw "Impossible de lire la version dans CMakeLists.txt"
    }
    $Version = $Matches[1]
}
if ($Version -notmatch '^\d+\.\d+(?:\.\d+)?$') { throw "Version invalide : $Version" }
$build = if ([System.IO.Path]::IsPathRooted($BuildDir)) { $BuildDir } else { Join-Path $root $BuildDir }
$output = if ([System.IO.Path]::IsPathRooted($OutputDir)) { $OutputDir } else { Join-Path $root $OutputDir }
$bin = Join-Path $build "Release"
foreach ($name in @("magicscan-tray.exe", "magicscan.exe")) {
    if (-not (Test-Path (Join-Path $bin $name))) { throw "Executable introuvable : $name" }
}
New-Item -ItemType Directory -Path $output -Force | Out-Null
$packageName = "MagicScan-$Version-win64"
$stage = Join-Path $output $packageName
if (Test-Path $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null
foreach ($name in @("magicscan-tray.exe", "magicscan.exe")) {
    Copy-Item -LiteralPath (Join-Path $bin $name) -Destination (Join-Path $stage $name)
}
Copy-Item -LiteralPath (Join-Path $root "docs\WINDOWS-PORTABLE.md") -Destination (Join-Path $stage "LISEZ-MOI.md")
$zip = Join-Path $output "$packageName.zip"
if (Test-Path $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal
Write-Host "Archive creee : $zip"

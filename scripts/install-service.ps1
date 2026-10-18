# Administrator-only opt-in installation of the read-only diagnostic service.
param([string]$Executable = ".\build\Release\magicscan-usb-service.exe")
$ErrorActionPreference = "Stop"
$principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Ouvrez PowerShell en tant qu'administrateur."
}
$source = (Resolve-Path -LiteralPath $Executable).Path
if (Get-Service -Name MagicScanUsb -ErrorAction SilentlyContinue) {
    throw "Le service MagicScanUsb existe deja. Desinstallez-le avant de le remplacer."
}
# A LocalSystem executable MUST NOT run directly from a development checkout
# or another directory writable by a standard user.
$targetDir = Join-Path $env:ProgramFiles "MagicScan\Service"
New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
$target = Join-Path $targetDir "magicscan-usb-service.exe"
Copy-Item -LiteralPath $source -Destination $target -Force
& sc.exe create MagicScanUsb "binPath= `"$target`"" "start= demand" "obj= LocalSystem" "DisplayName= MagicScan USB (diagnostic)"
if ($LASTEXITCODE -ne 0) { throw "Creation du service echouee." }
& sc.exe start MagicScanUsb
if ($LASTEXITCODE -ne 0) { throw "Demarrage du service echoue. Consultez sc.exe query MagicScanUsb." }
Write-Host "Service MagicScanUsb installe dans Program Files et demarre (demarrage manuel)."

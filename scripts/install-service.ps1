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
# Use the PowerShell SCM API wrapper, avoiding native sc.exe quoting rules.
# With no -Credential, New-Service uses the default LocalSystem account.
New-Service -Name "MagicScanUsb" -BinaryPathName ('"{0}"' -f $target) `
    -DisplayName "MagicScan USB (diagnostic)" -StartupType Manual -ErrorAction Stop | Out-Null
Start-Service -Name "MagicScanUsb" -ErrorAction Stop
Write-Host "Service MagicScanUsb installe dans Program Files et demarre (demarrage manuel)."

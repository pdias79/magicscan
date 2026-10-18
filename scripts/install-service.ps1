# Administrator-only opt-in installation of the read-only diagnostic service.
param([string]$Executable = ".\\build\\Release\\magicscan-usb-service.exe")
$ErrorActionPreference = "Stop"
$principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Ouvrez PowerShell en tant qu'administrateur."
}
$resolved = (Resolve-Path -LiteralPath $Executable).Path
& sc.exe query MagicScanUsb 2>$null | Out-Null
if ($LASTEXITCODE -eq 0) { throw "Le service MagicScanUsb existe deja. Desinstallez-le avant de le remplacer." }
& sc.exe create MagicScanUsb "binPath= `"$resolved`"" "start= demand" "obj= LocalSystem" "DisplayName= MagicScan USB (diagnostic)"
if ($LASTEXITCODE -ne 0) { throw "Creation du service echouee." }
& sc.exe start MagicScanUsb
if ($LASTEXITCODE -ne 0) { throw "Demarrage du service echoue. Consultez sc.exe query MagicScanUsb." }
Write-Host "Service MagicScanUsb installe et demarre (demarrage manuel)."

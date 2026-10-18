# Run elevated. Does not change disk ACLs or drivers.
$ErrorActionPreference = "Stop"
$principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Ouvrez PowerShell en tant qu'administrateur."
}
& sc.exe stop MagicScanUsb
& sc.exe delete MagicScanUsb
if ($LASTEXITCODE -ne 0) { throw "Suppression du service echouee." }
Write-Host "Service MagicScanUsb supprime. Aucun parametre USB modifie."

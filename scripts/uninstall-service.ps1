# Run elevated. Does not change disk ACLs or drivers.
$ErrorActionPreference = "Stop"
$principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw "Ouvrez PowerShell en tant qu'administrateur."
}
if (-not (Get-Service -Name MagicScanUsb -ErrorAction SilentlyContinue)) {
    Write-Host "Service MagicScanUsb deja absent."
    exit 0
}
& sc.exe stop MagicScanUsb
# Stop is asynchronous: wait for the service to leave the running state.
$service = Get-Service -Name MagicScanUsb
try { $service.WaitForStatus([ServiceProcess.ServiceControllerStatus]::Stopped, [TimeSpan]::FromSeconds(15)) }
catch { throw "Service toujours actif : attendez son arret avant suppression." }
& sc.exe delete MagicScanUsb
if ($LASTEXITCODE -ne 0) { throw "Suppression du service echouee." }
Write-Host "Service MagicScanUsb supprime. Aucun parametre USB modifie."
Write-Host "Le fichier sous Program Files peut etre supprime apres disparition du service."

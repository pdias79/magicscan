param(
    [Parameter(Mandatory=$true)][ValidateSet('Install','Uninstall')][string]$Action,
    [string]$ServiceExe = (Join-Path $env:ProgramFiles 'MagicScan\Service\magicscan-usb-service.exe')
)
$ErrorActionPreference = 'Stop'
$principal = [Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'L installation du service exige les droits administrateur.'
}
$name = 'MagicScanUsb'
if ($Action -eq 'Install') {
    $binary = (Resolve-Path -LiteralPath $ServiceExe).Path
    $existing = Get-Service -Name $name -ErrorAction SilentlyContinue
    if ($existing) {
        if ($existing.Status -ne 'Stopped') {
            Stop-Service -Name $name -ErrorAction Stop
            $existing.WaitForStatus([ServiceProcess.ServiceControllerStatus]::Stopped, [TimeSpan]::FromSeconds(20))
        }
        # An earlier manual service may point to a different binary.
        & sc.exe config $name 'binPath=' ('"{0}"' -f $binary) 'start=' 'auto'
        if ($LASTEXITCODE -ne 0) { throw 'Impossible de reconfigurer MagicScanUsb.' }
        Set-Service -Name $name -DisplayName 'MagicScan USB' -StartupType Automatic
    } else {
        New-Service -Name $name -BinaryPathName ('"{0}"' -f $binary) -DisplayName 'MagicScan USB' -StartupType Automatic | Out-Null
    }
    Start-Service -Name $name -ErrorAction Stop
    Write-Host 'MagicScanUsb installe et demarre automatiquement.'
} else {
    $existing = Get-Service -Name $name -ErrorAction SilentlyContinue
    if ($existing) {
        if ($existing.Status -ne 'Stopped') {
            Stop-Service -Name $name -ErrorAction Stop
            $existing.WaitForStatus([ServiceProcess.ServiceControllerStatus]::Stopped, [TimeSpan]::FromSeconds(20))
        }
        & sc.exe delete $name
        if ($LASTEXITCODE -ne 0) { throw 'Impossible de supprimer MagicScanUsb.' }
    }
    Write-Host 'MagicScanUsb supprime.'
}

param(
    [Parameter(Mandatory=$true)][string]$Source,
    [Parameter(Mandatory=$true)][string]$Destination
)
$ErrorActionPreference = 'Stop'
$text = [System.IO.File]::ReadAllText($Source, [System.Text.Encoding]::UTF8)
[System.IO.File]::WriteAllText($Destination, $text, [System.Text.Encoding]::Unicode)

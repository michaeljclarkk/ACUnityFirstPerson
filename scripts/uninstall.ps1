param(
    [Parameter(Mandatory = $true)]
    [string]$GamePath
)
$ErrorActionPreference='Stop'
if (Get-Process ACU -ErrorAction SilentlyContinue) { throw 'Close the game before removing the plugin.' }
$plugin=Join-Path $GamePath 'ACUFixes\plugins\ACUnityFirstPerson.dll'
if (Test-Path -LiteralPath $plugin) { Remove-Item -LiteralPath $plugin }
Write-Host 'First-person plugin removed. Settings and logs retained.'

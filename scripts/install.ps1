param(
    [Parameter(Mandatory = $true)]
    [string]$GamePath
)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
if (Get-Process ACU -ErrorAction SilentlyContinue) { throw 'Close the game before installing.' }
$exe=Join-Path $GamePath 'ACU.exe'
if (!(Test-Path -LiteralPath $exe)) { throw 'ACU.exe not found. Pass the game folder with -GamePath.' }
$expected=@('748ABDC6456397FE7D8080F975F2A5ED3123BCE548E7BF1C68B23F47E8B27E40','9725F1FB9E82521506007EE50763612D319A02EEFC48E09D8EB845DFF4BBCD87')
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -notin $expected) {
    throw 'Unsupported ACU.exe version. This build targets the executable version it was developed and tested against.'
}
$plugins=Join-Path $GamePath 'ACUFixes\plugins'
if (!(Test-Path -LiteralPath (Join-Path $GamePath 'ACUFixes\ACUFixes-PluginLoader.dll'))) { throw 'Install ACUFixes Plugin Loader first.' }
$dll=Join-Path $repo 'build\Release\ACUnityFirstPerson.dll'
if (!(Test-Path -LiteralPath $dll)) { throw 'Run scripts/build.ps1 first.' }
New-Item -ItemType Directory -Path $plugins -Force | Out-Null
$dest=Join-Path $plugins 'ACUnityFirstPerson.dll'
if (Test-Path -LiteralPath $dest) {
    $backup=Join-Path $repo ('backups\'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    Copy-Item -LiteralPath $dest -Destination $backup
}
Copy-Item -LiteralPath $dll -Destination $dest
$config=Join-Path $plugins 'ACUnityFirstPerson.ini'
if (!(Test-Path -LiteralPath $config)) { Copy-Item -LiteralPath (Join-Path $repo 'ACUnityFirstPerson.ini') -Destination $config }
if ((Get-FileHash -LiteralPath $dll).Hash -ne (Get-FileHash -LiteralPath $dest).Hash) { throw 'Installed DLL verification failed.' }
Copy-Item -LiteralPath (Join-Path $repo 'third_party\imgui\LICENSE.txt') -Destination (Join-Path $plugins 'ACUnityFirstPerson-ImGui-LICENSE.txt')
Write-Host 'Installed first person. Open its plugin menu to configure the toggle key and camera. Default key: period (.).'

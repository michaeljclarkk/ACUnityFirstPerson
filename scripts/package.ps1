param([switch]$SkipBuild)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot

# The version lives in CMakeLists.txt. Fail loudly if the plugin or INI drifts from it.
$cmake=Get-Content "$repo\CMakeLists.txt" -Raw
if ($cmake -notmatch 'project\(ACUnityFirstPerson VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'Cannot read the project version from CMakeLists.txt.' }
$version=$Matches[1]
$declared=@{
    'src/plugin.cpp'         = 'First Person '
    'ACUnityFirstPerson.ini' = 'ACUnityFirstPerson '
}
foreach ($file in $declared.Keys) {
    $match=[regex]::Match((Get-Content "$repo\$file" -Raw),[regex]::Escape($declared[$file])+'([0-9]+\.[0-9]+\.[0-9]+)')
    if (!$match.Success) { throw "$file does not declare a version string." }
    if ($match.Groups[1].Value -ne $version) { throw "$file says $($match.Groups[1].Value) but CMakeLists.txt says $version. Fix the mismatch before packaging." }
}
Write-Host "Packaging ACUnityFirstPerson $version."

if (!$SkipBuild) { & "$PSScriptRoot\build.ps1" }
$dll="$repo\build\Release\ACUnityFirstPerson.dll"
if (!(Test-Path -LiteralPath $dll)) { throw 'Release DLL not found. Run scripts\build.ps1 first.' }

# Stage the archive so it mirrors the layout users drop into the game folder.
$name="ACUnityFirstPerson-$version"
$package="$repo\build\package"
$stage=Join-Path $package $name
if (Test-Path -LiteralPath $package) { Remove-Item -LiteralPath $package -Recurse -Force }
$plugins=Join-Path $stage 'ACUFixes\plugins'
New-Item -ItemType Directory -Path $plugins -Force | Out-Null
Copy-Item -LiteralPath $dll -Destination (Join-Path $plugins 'ACUnityFirstPerson.dll')
Copy-Item -LiteralPath "$repo\ACUnityFirstPerson.ini" -Destination (Join-Path $plugins 'ACUnityFirstPerson.ini')
Copy-Item -LiteralPath "$repo\third_party\imgui\LICENSE.txt" -Destination (Join-Path $stage 'ACUnityFirstPerson-ImGui-LICENSE.txt')
Copy-Item -LiteralPath "$repo\LICENSE" -Destination (Join-Path $stage 'LICENSE')
Copy-Item -LiteralPath "$repo\README.md" -Destination (Join-Path $stage 'README.md')
Copy-Item -LiteralPath "$repo\CHANGELOG.md" -Destination (Join-Path $stage 'CHANGELOG.md')

# Only the staged files may ship: no build output, captures or research notes.
$expected=@(
    "$name\ACUFixes\plugins\ACUnityFirstPerson.dll"
    "$name\ACUFixes\plugins\ACUnityFirstPerson.ini"
    "$name\ACUnityFirstPerson-ImGui-LICENSE.txt"
    "$name\LICENSE"
    "$name\README.md"
    "$name\CHANGELOG.md"
)
$staged=(Get-ChildItem $stage -Recurse -File | ForEach-Object { $_.FullName.Substring($package.Length+1) })
$extra=@($staged | Where-Object { $expected -notcontains $_ })
if ($extra.Count -gt 0) { throw "Unexpected files in the release payload: $($extra -join ', ')" }
$missing=@($expected | Where-Object { $staged -notcontains $_ })
if ($missing.Count -gt 0) { throw "Missing files from the release payload: $($missing -join ', ')" }

$dist="$repo\dist"
New-Item -ItemType Directory -Path $dist -Force | Out-Null
$zip=Join-Path $dist "$name.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
[System.IO.Compression.ZipFile]::CreateFromDirectory($stage,$zip,[System.IO.Compression.CompressionLevel]::Optimal,$true)

$archive=[System.IO.Compression.ZipFile]::OpenRead($zip)
try { $entries=@($archive.Entries | ForEach-Object { $_.FullName }) } finally { $archive.Dispose() }
Write-Host "Created $zip ($([math]::Round((Get-Item -LiteralPath $zip).Length/1KB,1)) KB)"
$entries | Sort-Object | ForEach-Object { Write-Host "  $_" }
Write-Host 'Add this ZIP to Nexus with the ACUFixes Plugin Loader listed as a requirement.'

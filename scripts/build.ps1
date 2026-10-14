param([ValidateSet('Release','Debug')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
cmake --fresh -S $repo -B "$repo\build" -G "Visual Studio 18 2026" -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
cmake --build "$repo\build" --config $Configuration
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
ctest --test-dir "$repo\build" -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }

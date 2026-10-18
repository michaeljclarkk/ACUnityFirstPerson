# ACUnityFirstPerson

C++17 Windows x64 DLL plugin for the Assassin's Creed Unity ACUFixes Plugin Loader.

## Structure

- `src/plugin.cpp`: plugin entry point, game hooks, camera updates, and settings menu.
- `src/plugin_api.h`: plugin-loader ABI declarations.
- `src/camera_math.h`, `src/camera_comfort.h`, `src/head_anchor.h`: camera geometry, camera state, and head tracking.
- `src/combat_camera.h`, `src/body_motion.h`, `src/head_visibility.h`: combat framing, torso-following motion, and renderer visibility flags.
- `src/settings.h`: settings validation and persistence.
- `tests/`: CTest test executables.
- `third_party/imgui/`: vendored Dear ImGui and its license.
- `scripts/`: build, installation, removal, and packaging scripts.
- `ACUnityFirstPerson.ini`: default configuration.
- `README.md` / `CHANGELOG.md` / `LICENSE`: user documentation, development history, MIT license.
- `.github/workflows/release.yml`: tag-driven release. There is no other CI.
- `external/`: reference checkouts.
- `build/`: generated build output; `dist/`: packaged release archives; `backups/`: installation backups.

Author-only working notes and local instrumentation are gitignored and never published.
`scripts/package.ps1` enforces that by allowlisting the release payload.

## Build and test

Requires Windows x64, Visual Studio 2026 C++ tools, a Windows SDK, and CMake on PATH.
Run from the repository root in PowerShell:

```powershell
.\scripts\build.ps1
```

This configures the Visual Studio 18 2026 x64 generator, builds Release, and runs CTest.
Output: `build/Release/ACUnityFirstPerson.dll`.

To rerun tests:

```powershell
ctest --test-dir build -C Release --output-on-failure
```

Use `-Configuration Debug` with the build script for a Debug build.

To produce the Nexus release archive (builds, verifies the version strings agree, then writes
`dist/ACUnityFirstPerson-<version>.zip`):

```powershell
.\scripts\package.ps1
```

The installer and uninstaller take a mandatory `-GamePath`, e.g.
`.\scripts\install.ps1 -GamePath "D:\Steam\steamapps\common\Assassin's Creed Unity"`.



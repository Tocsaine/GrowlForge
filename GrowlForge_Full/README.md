# GrowlForge 1.0.1 — CLAP guitar effect

GrowlForge is a post-amplifier tone-shaping CLAP effect for modern heavy guitar sounds.

Recommended chain:

```text
Guitar → amplifier → GrowlForge → cabinet / IR loader
```

It is not a model of a proprietary amplifier or artist preset. It adds growl, restrained fuzz, tightening, presence shaping and optional pre-cabinet high-frequency filtering to an already amplified signal.

## Build requirements on Windows

Install Visual Studio with:

- Desktop development with C++
- C++ CMake tools for Windows
- Windows 10 or Windows 11 SDK

CMake 3.24 or newer is required.

## Recommended Windows build

Open PowerShell inside `GrowlForge_Full` and run:

```powershell
.\build-windows.ps1
```

The script:

1. verifies that Windows is 64-bit;
2. removes a stale CMake cache when `-Clean` is used;
3. configures the Visual Studio 2026 x64 preset;
4. builds Release;
5. verifies that `GrowlForge.clap` was created.

For a completely clean rebuild:

```powershell
.\build-windows.ps1 -Clean
```

The result is:

```text
out\build\windows-x64-release\plugins\GrowlForge.clap
```

### Visual Studio 2022

Use:

```powershell
cmake --preset windows-x64-release-vs2022
cmake --build --preset windows-x64-release-vs2022
```

## Manual Visual Studio 2026 build

```powershell
cmake --preset windows-x64-release
cmake --build --preset windows-x64-release
```

The preset explicitly selects `x64`. CMake also rejects a 32-bit Windows configuration before linking, so the previous x86/x64 runtime-library conflict should no longer produce a long list of unresolved symbols.

## Install for REAPER

Close REAPER, then copy:

```text
out\build\windows-x64-release\plugins\GrowlForge.clap
```

to:

```text
%LOCALAPPDATA%\Programs\Common\CLAP
```

Restart REAPER and perform a plug-in rescan.

## GitHub Actions

`.github/workflows/windows-build.yml` builds the plug-in on a clean Windows x64 runner for every push and pull request affecting `GrowlForge_Full`.

The workflow uploads `GrowlForge.clap` as a downloadable Actions artifact.

## Current parameters

Input, Drive, Fuzz, Growl, Tight, Presence, Cab Filter, Gate, Mix and Output.

## Current limitations

- No custom GUI.
- Float32 audio path only.
- Stereo main input/output.
- `Cab Filter` is a lightweight low-pass filter, not an IR loader.
- The bundled `clap.h` is a minimal compatibility header, not the complete official CLAP SDK.

# Troubleshooting

## `library machine type x86 conflicts with target x64`

This means an old or incorrectly configured build directory is being reused.

Run:

```powershell
.\build-windows.ps1 -Clean
```

The Windows presets explicitly request x64, and CMake now stops immediately if the configured pointer size is not 8 bytes.

## Visual Studio keeps using an old cache

Close Visual Studio, then remove:

```text
out
.vs
```

Reopen the folder and select `windows-x64-release`.

## CMake cannot find `Visual Studio 18 2026`

Confirm Visual Studio 2026 and its C++ workload are installed.

For Visual Studio 2022 use:

```powershell
cmake --preset windows-x64-release-vs2022
cmake --build --preset windows-x64-release-vs2022
```

## Ninja reports dirty timestamps

The main Windows build no longer uses Ninja. If using `portable-release`, synchronize the system clock and delete its build directory.

## Build succeeds but REAPER does not list the plug-in

1. Close REAPER.
2. Copy `GrowlForge.clap` to `%LOCALAPPDATA%\Programs\Common\CLAP`.
3. Restart REAPER.
4. Clear and rescan the plug-in cache.
5. Search for `GrowlForge`.

# Updating the working copy from GitHub

## First installation on a new machine

```powershell
cd D:\Projects
git clone https://github.com/Tocsaine/GrowlForge.git
cd GrowlForge\GrowlForge_Full
.\build-windows.ps1 -Clean
```

## Normal update

Close REAPER before replacing the plug-in.

```powershell
cd D:\Projects\GrowlForge
git status
git pull --ff-only
cd GrowlForge_Full
.\build-windows.ps1
```

Use `git status` first. If it reports local modifications, do not run a destructive reset. Commit, stash or copy those changes before pulling.

## Update after CMake or preset changes

```powershell
cd D:\Projects\GrowlForge
git pull --ff-only
cd GrowlForge_Full
.\build-windows.ps1 -Clean
```

## Install the new build

```powershell
$source = ".\out\build\windows-x64-release\plugins\GrowlForge.clap"
$target = "$env:LOCALAPPDATA\Programs\Common\CLAP"

New-Item -ItemType Directory -Force $target | Out-Null
Copy-Item -Force $source $target
```

Restart REAPER and rescan plug-ins.

## Confirm which revision is installed locally

```powershell
git log -1 --oneline
git status
```

`git status` should report a clean working tree after a normal update.

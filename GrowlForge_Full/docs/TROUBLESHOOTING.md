# Troubleshooting

## Visual Studio keeps using an old broken cache

Close Visual Studio and delete the project `out` and `.vs` folders, then reopen the project folder.

## Build succeeds but REAPER does not list the plug-in

Confirm that the file ends in `.clap`, copy it to `%LOCALAPPDATA%\\Programs\\Common\\CLAP`, restart REAPER, and perform a clear/rescan.

## CMake cannot configure

This project has no downloaded dependencies. Check that **Desktop development with C++** and **C++ CMake tools for Windows** are installed in Visual Studio Installer.

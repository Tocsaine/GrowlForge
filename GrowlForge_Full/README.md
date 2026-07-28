# GrowlForge 1.0 — CLAP guitar effect

GrowlForge is a self-contained CLAP audio effect for modern heavy guitar tones. It does not copy any proprietary amplifier or artist preset. Its design target is a tight, low-tuned, growling high-gain sound with a restrained fuzzy edge.

## DSP chain

1. Input gain
2. Envelope gate
3. Tight variable high-pass
4. Low/body split and growl emphasis
5. Asymmetric soft-clipping drive
6. Parallel restrained fuzz
7. Presence shaping
8. Cabinet-style low-pass
9. Wet/dry and output gain

## Parameters

Input, Drive, Fuzz, Growl, Tight, Presence, Cab Filter, Gate, Mix, Output.

Default values are the intended starting preset:

- Input: 0 dB
- Drive: 4.8
- Fuzz: 3.2
- Growl: 6.3
- Tight: 6.8
- Presence: 5.4
- Cab Filter: 7.0
- Gate: -58 dB
- Mix: 100%
- Output: -5 dB

## Build in Visual Studio

1. Extract the project to a normal local path, preferably without OneDrive synchronization.
2. In Visual Studio choose **File > Open > Folder** and select this directory.
3. Select **windows-x64-release** in the configuration preset selector.
4. Wait for CMake configuration to finish.
5. Choose **Build > Build All**.
6. The result is normally located at:
   `out/build/windows-x64-release/plugins/GrowlForge.clap`

There is no FetchContent, network access, Git operation, or SDK generation during configuration.

## Install for REAPER on Windows

Copy `GrowlForge.clap` to one of these folders:

- `%LOCALAPPDATA%\\Programs\\Common\\CLAP`
- `%COMMONPROGRAMFILES%\\CLAP`

Then in REAPER open **Options > Preferences > Plug-ins > VST**, clear/rescan the plug-in cache, and search for GrowlForge.

## Current limitations

- No custom GUI; REAPER displays the host-generated parameter interface.
- Float32 audio path only.
- Stereo main input/output.
- The cabinet section is a lightweight filter, not an IR loader.

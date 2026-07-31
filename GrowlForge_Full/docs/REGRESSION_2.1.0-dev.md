# 2.1.0-dev architecture regression report

Reference: GrowlForge 2.0.3 built from the uploaded source.

Candidate: modular 2.1.0-dev architecture build.

Environment used for the comparison:

- Linux x86-64
- GCC 14.2
- C++20 Release build
- 48 kHz
- 128-sample blocks
- stereo 32-bit float processing

## Audio comparison

Eight three-second scenarios were rendered through both plug-ins:

1. neutral defaults;
2. Drive 5;
3. Drive/Grind/Fuzz/Harmonic Bias combination;
4. Gate and tone-shaping combination;
5. Motion/dynamics combination;
6. Auto-Gain, Parallel Dry, Output and Ceiling combination;
7. broad all-section stress configuration with `×2`;
8. Drive 10 with `×2` enabled, confirming Drive remains excluded.

Every generated float sample was bit-identical:

```text
max absolute difference: 0
RMS difference:          0
non-zero sample count:   0
```

## Parameter contract

The following were compared through the CLAP parameter extension and were identical:

- plug-in ID and name;
- parameter count;
- IDs;
- names and modules;
- flags;
- minimum, maximum and default values.

The user-visible version string is the only descriptor change: `2.0.3` → `2.1.0-dev`.

## State tests

- A non-default state containing all persisted controls produced an identical 296-byte state blob.
- Saved-state SHA-256 for both builds:

```text
1743bc8a33833a5daff103aab1bcde712dc5021bd2dcbff561dacce007f6b7a1
```

- State versions 7, 8, 9, and 10 were loaded into both builds and rendered. Every resulting sample was bit-identical.

## Build status

- Modular portable Release build: successful.
- Win32 GUI source was preserved and separated into its own translation unit.
- A native MSVC/Windows SDK build cannot be executed in the Linux verification environment; the project retains the existing Visual Studio 2022/2026 presets for local Windows validation.

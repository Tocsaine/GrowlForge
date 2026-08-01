# GrowlForge 2.2.1

GrowlForge 2.2 focuses on fast personal workflow without changing the DSP character introduced in 2.1.

## Main changes

- two persistent A/B sound slots;
- Copy A→B and B→A;
- 64-step snapshot Undo/Redo for GUI edits and preset loads;
- smart Save, Save As, Rename, Delete, Refresh, and Open Preset Folder commands;
- Save / Discard / Cancel protection before replacing a modified preset;
- six new personal starter presets, for a total factory bank of sixteen;
- project-state format 12, preserving A/B slots and the active slot;
- state versions 7–11 remain loadable.

The DSP, plugin ID, parameter IDs, Auto-Gain 2.0, Gate, Bypass, meters, and Drive behavior are unchanged from 2.1.0.

See `RELEASE_2.2.1.md`, `docs/ARCHITECTURE.md`, and `docs/PRESETS.md`.

## Build

```powershell
cd GrowlForge_Full
.\build-windows.ps1 -Clean -VisualStudio vs2022
```

Copy the resulting `GrowlForge.clap` to:

```text
%LOCALAPPDATA%\Programs\Common\CLAP
```

Then restart the DAW and run a full CLAP rescan.

## Signal position

```text
Amp / amp sim → GrowlForge → Cab / IR
```

## Drive subsonic/DC fix in 2.0.3

- Removed the non-zero zero-input response from the asymmetric Drive transfer.
- Added a Drive-only 20 Hz second-order Butterworth high-pass after saturation.
- Eliminates the large DC / approximately 10 Hz energy visible in analyzers.
- The filter is inactive and reset at `Drive = 0`; other controls are unchanged.
- GUI performance and layout improvements from 2.0.2 are retained.

## GUI performance and layout fix in 2.0.2

- Replaced per-frame backbuffer allocation with persistent static and dynamic surfaces.
- Split the interface into a cached static layer and small dynamic repaint regions.
- Cached knob bodies, fonts, string formats, and reusable GDI+ drawing objects.
- The 30 Hz timer now redraws only live meters and activity indicators.
- Mouse movement invalidates only the affected control and parameter display.
- GUI animation stops while the editor is hidden.
- Fixed clipping and edge alignment of the bottom dB scale labels.
- Audio DSP, parameter IDs, state format, and v1.4.4 sound are unchanged.


## Build fix in 2.0.1

Fixed MSVC overload ambiguity in GDI+ rectangle drawing calls by using explicit `Gdiplus::RectF` overloads. Audio DSP and parameter behavior are unchanged.

## GrowlForge 2.0 custom interface

Version 2.0 adds a native scalable Win32 interface through the CLAP GUI extension.
The DSP, parameter IDs, ranges and state version remain compatible with 1.4.4.

Highlights:

- one-screen layout with Input & Feel, Distortion Core, Motion & Dynamics,
  Tone & Texture, and Routing & Output sections;
- large central Drive control and visually separate Fuzz control;
- stereo input/output meters and real activity meters;
- host automation gestures for GUI edits;
- Shift fine control, double-click reset and mouse-wheel adjustment;
- 1200 × 720 default size with 5:3 scalable vector rendering.

See `docs/GUI.md` and `ANNOUNCEMENT_RU.md`.

Post-amp guitar enhancer:

```text
Amp / amp sim → GrowlForge → Cab / IR
```

## Neutral default

A new instance is neutral:

```text
Input Trim 0 dB
Gate 0
Tight through Pre-Cab Filter 0
Parallel Dry 0%
Output 0 dB
Ceiling 0 dB (disabled)
Auto-Gain Off
```

With those values the plug-in returns the input sample unchanged.

## Enhancer controls

All tone controls from Tight through Pre-Cab Filter use:

```text
0 = no effect
10 = maximum effect
```

Mass, Growl and Bite now also alter harmonic structure before 4× oversampled saturation.

## Auto-Gain

Auto-Gain compares slow RMS levels of the dry and processed paths and applies a smoothed correction up to ±12 dB. It settles over roughly 0.2–0.4 seconds.

## Installation

```powershell
cd GrowlForge_Full
.\build-windows.ps1 -Clean
```

For Visual Studio 2022:

```powershell
.\build-windows.ps1 -Clean -VisualStudio vs2022
```

Copy `out\build\<preset>\plugins\GrowlForge.clap` to:

```text
%LOCALAPPDATA%\Programs\Common\CLAP
```

Restart the DAW and run a full CLAP rescan.

## Compatibility

Version 1.2.0 adds Auto-Gain and uses a new 19-parameter state format.


## Precision and Auto-Gain display

- Continuous controls are quantized to `0.1`.
- Hosts that expose CLAP text editing can pass exact keyboard-entered values.
- `Auto-Gain Correction` shows the current smoothed correction in dB.
- Set `Apply Auto-Gain` to `Apply` to add that correction to `Output`, disable Auto-Gain, and reset the temporary correction.


## 1.2.2 changes

- `Input Trim` range: `-12…+12 dB`, default and center `0 dB`.
- `Output` range: `-12…+12 dB`, default and center `0 dB`.
- `Apply Auto-Gain` now writes the current absolute correction to `Output` instead of adding it repeatedly.
- Auto-Gain RMS history and internal gain are reset whenever Auto-Gain is toggled or committed.


## 1.2.3 change

When `Auto-Gain` is enabled, `Output` is immediately reset to `0 dB`.
This gives the loudness matcher a neutral reference and prevents previously
committed or manually entered Output gain from contaminating the measurement.
The internal RMS history is reset at the same time.


## 1.2.4 voicing changes

- `Body` is substantially stronger, especially from the middle to maximum range.
- `Punch` produces a more obvious attack and low-mid impact.
- `Tight` keeps a familiar lower range but accelerates above roughly `7`.
- `Drive`, `Grind`, and `Fuzz` become more exaggerated from roughly `8–10`.
- The design target remains: `0` is neutral, while `10` is intentionally oversized.


## 1.3.0 additive controls

The v1.2.4 voicing and all existing parameter IDs are preserved. Seven controls
were appended; every one defaults to `0`, and at `0` its DSP contribution is zero.

- `Bloom`: adds harmonic growth during note decay.
- `Sag`: adds level-dependent power-supply-style compression and recovery.
- `Dynamics`: increases the dependence of saturation on picking strength.
- `Texture`: adds progressively rougher, grainier nonlinear detail.
- `Focus`: concentrates the nonlinear feed toward the midrange; it does not alter
  the existing enhancer controls when set to `0`.
- `Attack`: emphasizes detected note transients.
- `Stereo Width`: adds subtle opposite-polarity high-frequency decorrelation.
  It is inactive at `0` and is intended for stereo material.

State files from v1.2.4/state version 7 are accepted. New parameters load at zero
when an older state is restored.


## 1.3.1 changes

- Removed `Stereo Width` from the plugin.
- Reworked `Attack` into a stronger two-stage transient enhancer:
  - fast edge emphasis for pick definition;
  - short envelope-shaped body emphasis for a more audible attack.
- The maximum Attack effect is roughly twice as strong as in v1.3.0.
- All other controls and the v1.3.0 voicing remain unchanged.
- State versions 7 and 8 remain loadable. The removed Stereo Width value is ignored.


## 1.4.0

Added without changing parameter IDs 0-26 or the DSP path when the new controls are neutral:

- Resonance: dynamic 110 Hz cabinet-style low resonance.
- Compression: parallel glue compression that preserves the initial pick attack.
- Harmonic Bias: progressively adds asymmetric/even-harmonic coloration.
- x2: doubles artistic/coloring depths while leaving Input, Output, Gate, Focus, Parallel Dry, Ceiling, Auto-Gain and Pre-Cab unchanged.
- Indicator section: live read-only Saturation, Bloom, Compression, Sag and Attack activity meters for hosts using the generic CLAP parameter interface.

All new controls default to zero/off. State versions 7, 8 and 9 remain loadable.


## 1.4.1 Drive redesign

- Reworked only the Drive stage; no other control algorithm was intentionally changed.
- Added restrained broad-mid pre-emphasis inside Drive.
- Added a blend of symmetric and asymmetric saturation that evolves with Drive.
- Added short transient-sensitive drive pressure for stronger pick response.
- Preserved a predictable, near-linear 0-10 control range.
- x2 now increases Drive character and touch response instead of doubling raw Drive gain.
- Compression interaction was deliberately not added.
- Drive at 0 fully bypasses the new Drive-specific processing.


## 1.4.2 Drive range and distortion update

- Rebalanced Drive so the middle of the 0-10 range is clearly audible on a clean input.
- Increased internal pre-gain and wet contribution without turning the control into an output boost.
- Added a smoothly blended second clipping stage above roughly Drive 5.5.
- Maximum Drive now approaches controlled distortion while remaining distinct from Fuzz.
- Preserved pick attack by reducing second-stage pressure during detected transients.
- Added subtle low-end cleanup and broader mid emphasis at high Drive values.
- x2 increases the harder-stage blend, asymmetry and touch pressure instead of doubling output level.
- Compression and every non-Drive control remain intentionally unchanged.


## 1.4.3 x2 Drive level correction

- Fixed the loudness dip that could occur when Drive was increased with x2 enabled.
- Added a smooth, bounded makeup correction tied only to the extra x2 hard-stage saturation.
- The correction is exactly neutral at Drive 0 and when x2 is disabled.
- Drive clipping, transient relief, Compression and all other controls remain unchanged.

## 1.4.4 x2 / Drive separation

- Removed every direct x2 modifier from Drive.
- Drive pre-emphasis, transient pressure, asymmetry, hard-stage blend and hard-stage input now use the same values whether x2 is off or on.
- Removed the x2-specific Drive makeup compensation introduced in 1.4.3 because it is no longer required.
- The normal x2-off Drive sound from 1.4.3 is preserved unchanged.
- x2 continues to affect the other artistic/coloring controls as before.
- Parameter IDs and state format are unchanged.


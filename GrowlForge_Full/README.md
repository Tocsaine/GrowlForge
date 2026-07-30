# GrowlForge 1.2.1

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

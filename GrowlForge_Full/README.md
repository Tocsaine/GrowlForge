# GrowlForge 1.1.0

GrowlForge is a post-amplifier guitar tone sculptor for CLAP hosts.

Recommended chain:

```text
Guitar → amp sim / real amp capture → GrowlForge → cabinet / IR → final EQ
```

## What changed in 1.1.0

- 4× oversampling around the nonlinear saturation stage.
- Four-pole internal anti-alias filtering.
- Additional two-pole post smoothing.
- Lower default input and output levels for post-amp use.
- Drive-dependent gain compensation.
- Output ceiling control.
- Expanded tone controls for tight, aggressive and massive guitar sounds.
- Fixed a null-pointer ordering issue in the process callback.
- Version bumped to 1.1.0.

## Parameters

### Gain and dynamics

- **Input Trim** — level entering GrowlForge. Default is `-12 dB`.
- **Gate** — input envelope gate.
- **Output** — final output trim. Default is `-12 dB`.
- **Ceiling** — smooth output ceiling. Default is `-1 dB`.
- **Mix** — dry/wet blend.

### Low end and weight

- **Tight** — raises the high-pass frequency and reduces loose deep lows.
- **Punch** — emphasizes the focused low-mid attack.
- **Body** — adds thickness without only boosting sub-bass.
- **Mass** — restores controlled deep weight after tightening.

### Midrange and saturation

- **Growl** — focused aggressive midrange.
- **Drive** — main saturation amount.
- **Grind** — sharper asymmetric harmonic texture.
- **Fuzz** — restrained parallel fuzz layer.

### High-frequency character

- **Bite** — upper-mid articulation.
- **Presence** — broad high-frequency projection.
- **Air** — restrained top-end openness.
- **Smooth** — increases internal anti-alias damping.
- **Pre-Cab Filter** — mild low-pass intended before a separate cabinet or IR.

## Starting points

### Tight modern rhythm

```text
Input Trim  -12 dB
Tight       7.0
Punch       6.5
Body        4.0
Mass        3.5
Growl       6.5
Drive       2.5
Grind       3.5
Fuzz        0.8
Bite        5.0
Presence    4.0
Air         2.0
Smooth      7.0
Output      -12 dB
Ceiling     -1 dB
```

### Massive wide rhythm

```text
Input Trim  -14 dB
Tight       4.0
Punch       5.0
Body        7.0
Mass        7.5
Growl       5.0
Drive       2.0
Grind       2.0
Fuzz        1.5
Bite        3.5
Presence    3.0
Air         2.5
Smooth      7.5
Output      -13 dB
Ceiling     -1 dB
```

### Aggressive growl

```text
Input Trim  -13 dB
Tight       6.0
Punch       5.5
Body        4.5
Mass        4.5
Growl       8.0
Drive       3.0
Grind       5.5
Fuzz        1.5
Bite        5.5
Presence    4.5
Air         1.5
Smooth      7.0
Output      -13 dB
Ceiling     -1 dB
```

## Build on Windows

Visual Studio 2026:

```powershell
.\build-windows.ps1 -Clean
```

Visual Studio 2022:

```powershell
.\build-windows.ps1 -Clean -VisualStudio vs2022
```

The resulting plug-in is located under the selected preset's:

```text
out\build\<preset>\plugins\GrowlForge.clap
```

## Compatibility note

Version 1.1.0 uses a new 18-parameter state layout. Presets or projects saved by the earlier 10-parameter prototype may need to be recreated.

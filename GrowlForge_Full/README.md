# GrowlForge 1.1.2

GrowlForge is a post-amplifier guitar tone sculptor for CLAP hosts.

```text
Guitar → amp sim / real amp capture → GrowlForge → cabinet / IR → final EQ
```

## Changes in 1.1.2

### Stronger tone response

- **Tight** — slightly stronger low-end cleanup and higher maximum high-pass point.
- **Body** — slightly wider thickness range.
- **Mass** — slightly stronger deep-weight range.
- **Growl** — substantially stronger focused midrange transformation.
- **Drive** — medium increase in saturation range.
- **Grind** — medium increase in asymmetry and metallic harmonic character.
- **Bite** — substantially stronger upper-mid attack.
- **Presence** — substantially stronger broad projection.
- **Air** — now clearly moves between darker and more open top end.
- **Smooth** — now applies an audible fizz-reduction contour in addition to internal anti-alias filtering.
- **Pre-Cab Filter** — expanded from nearly open down to approximately 3.5 kHz.

### Adaptive Fuzz

Fuzz was redesigned as a more controlled layer:

- deep lows remain mostly in the main saturation path;
- fuzz is focused more on mid and high content;
- the blend increases on sustained signal;
- pick transients retain more of the normal Drive/Grind character;
- low-frequency foundation is retained instead of being fully fuzzed.

This makes low and medium Fuzz settings more useful after an amplifier while preserving a clearly audible effect at high settings.

## Defaults

```text
Input Trim  -6 dB
Output       0 dB
Ceiling     -1 dB
```

## Testing the controls

Use the same riff and compare:

```text
Tight:          0 / 5 / 10
Body:           0 / 5 / 10
Mass:           0 / 5 / 10
Growl:          0 / 5 / 10
Drive:          0 / 5 / 10
Grind:          0 / 5 / 10
Fuzz:           0 / 3 / 7 / 10
Bite:           0 / 5 / 10
Presence:       0 / 5 / 10
Air:            0 / 5 / 10
Smooth:         0 / 5 / 10
Pre-Cab Filter: 0 / 5 / 10
```

`Smooth` and `Pre-Cab Filter` intentionally overlap only partly:

- **Smooth** reduces fizz and hard upper harmonics while retaining more articulation.
- **Pre-Cab Filter** changes the broad upper bandwidth and can become deliberately dark.

## Build

Visual Studio 2026:

```powershell
.\build-windows.ps1 -Clean
```

Visual Studio 2022:

```powershell
.\build-windows.ps1 -Clean -VisualStudio vs2022
```

## Compatibility

Version 1.1.2 keeps the same 18 parameter IDs and state layout as 1.1.0–1.1.1.

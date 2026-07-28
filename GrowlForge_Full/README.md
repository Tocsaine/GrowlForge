# GrowlForge 1.1.1

GrowlForge is a post-amplifier guitar tone sculptor for CLAP hosts.

Recommended chain:

```text
Guitar → amp sim / real amp capture → GrowlForge → cabinet / IR → final EQ
```

## Changes in 1.1.1

- Every tone control now has a substantially wider and more obvious range.
- `Input Trim` default changed from `-12 dB` to `-6 dB`.
- `Output` default changed from `-12 dB` to `0 dB`.
- `Ceiling` remains at `-1 dB` to protect the following cabinet/IR stage.
- Drive, Grind and Fuzz now move from subtle coloration to clearly audible destruction.
- Tight, Punch, Body, Mass and Growl now produce much larger spectral changes.
- Bite, Presence and Air are stronger, while oversampling and smoothing remain active.
- Pre-Cab Filter and Smooth now cover a wider range.

## Control behavior

### Low end

- **Tight** — dramatically removes loose low end and raises the input high-pass.
- **Punch** — strongly emphasizes focused low-mid attack.
- **Body** — adds broad thickness and density.
- **Mass** — adds deep controlled weight and can substantially reshape the low end.

### Midrange and saturation

- **Growl** — moves from scooped/neutral to strongly forward aggressive mids.
- **Drive** — main saturation, now with a much wider gain range.
- **Grind** — asymmetric metallic texture and sharper odd-harmonic character.
- **Fuzz** — parallel fuzz layer, now capable of very obvious fuzz coloration.

### High frequencies

- **Bite** — strong upper-mid attack and pick definition.
- **Presence** — broad projection and edge.
- **Air** — top-end openness; use carefully after bright amp sims.
- **Smooth** — stronger anti-alias and high-frequency damping.
- **Pre-Cab Filter** — broad pre-cab low-pass range.

## Default gain staging

```text
Input Trim  -6 dB
Output       0 dB
Ceiling     -1 dB
```

`Output` is now unity by default, but `Ceiling` still catches excessive peaks. For correct comparison, level-match bypass and enabled states using `Input Trim` first.

## Suggested test procedure

Set all character controls to `5`, then move one control from `0` to `10` while playing the same riff. Every control should now produce an obvious change.

For the strongest comparison:

```text
Drive / Grind / Fuzz: test separately
Tight / Mass: compare as opposite low-end directions
Punch / Body: compare attack versus width
Growl: compare 0, 5 and 10
Bite / Presence / Air: compare separately after the same cab IR
Smooth / Pre-Cab Filter: compare at 0 and 10
```

## Build

Visual Studio 2026:

```powershell
.\build-windows.ps1 -Clean
```

Visual Studio 2022:

```powershell
.\build-windows.ps1 -Clean -VisualStudio vs2022
```

The resulting file is under:

```text
out\build\<preset>\plugins\GrowlForge.clap
```

## Compatibility

Version 1.1.1 keeps the 18-parameter state layout introduced in 1.1.0.

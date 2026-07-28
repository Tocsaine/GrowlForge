# Troubleshooting GrowlForge 1.1.0

## Too much high-frequency fizz

Start with:

```text
Smooth: 7.0
Pre-Cab Filter: 3.0
Air: 1.5
Presence: 3.5
Bite: 4.0
```

Then inspect the amp sim. GrowlForge now suppresses nonlinear aliasing internally, but it cannot remove aliasing already generated upstream by another plug-in.

## Output is still too loud

Use the controls in this order:

1. Set `Input Trim` so bypass and enabled levels are comparable.
2. Set `Output` for chain gain staging.
3. Keep `Ceiling` between `-2 dB` and `-1 dB`.
4. Do not use the ceiling as the primary volume control.

## Build error mentioning x86

Run:

```powershell
.\build-windows.ps1 -Clean
```

## Old projects do not restore parameters

Version 1.1.0 changed from 10 to 18 parameters and uses a new state format. Recreate the GrowlForge settings in the project and save it again.

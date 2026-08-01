# GrowlForge 2.2.0 — Workflow

GrowlForge 2.2 adds a personal workflow layer while leaving the 2.1 DSP unchanged.

## A/B

- Two independent sound snapshots: A and B.
- Switching slots stores the current slot before recalling the other one.
- `A→B` and `B→A` copy buttons.
- The active slot is shown in the top display.
- A/B state is restored with the DAW project.

## Undo / Redo

- Up to 64 complete sound snapshots.
- One knob drag is one history action.
- Toggle changes, wheel changes, double-click resets, and preset loads are tracked.
- Keyboard shortcuts: `Ctrl+Z`, `Ctrl+Y`, `Ctrl+Shift+Z`.

## Presets

- Smart Save overwrites an existing user preset or opens Save As for factory presets.
- Save As, Rename, Delete, Refresh, and Open Preset Folder are available from the preset menu.
- Modified presets are protected with Save / Discard / Cancel before another preset replaces them.
- Preset overwrite uses a temporary file and backup replacement.
- Six new personal factory presets were added.

## State compatibility

- Project state version: 12.
- Versions 7–11 remain loadable.
- Version 12 stores A, B, and the active slot.
- User `.gfpreset` files remain separate from project state.

## Preserved behavior

- No DSP formulas changed.
- Parameter IDs and ranges are unchanged.
- Drive remains excluded from ×2.
- Auto-Gain 2.0, Bypass, Gate, metering, and the Drive DC/subsonic fix are unchanged.

# GrowlForge presets — 2.2

## Factory bank

GrowlForge 2.2 contains sixteen embedded factory presets. The original ten remain, plus six personal starting points:

- Tactile Crunch
- Dense but Clear
- Low String Clamp
- Velvet Violence
- Glass Teeth
- Living Fuzz

Use the left/right arrows to cycle presets. Click the preset name to open the complete preset menu.

## User preset workflow

`SAVE` now behaves intelligently:

- a user preset is overwritten in place;
- a factory preset or project state opens Save As.

The preset menu also provides:

- Save As;
- Rename;
- Delete;
- Refresh preset list;
- Open preset folder.

Before loading another preset, GrowlForge asks whether modified settings should be saved, discarded, or kept by cancelling the operation.

User presets use `.gfpreset` and are stored by default in:

```text
%APPDATA%\GrowlForge\Presets
```

Preset writes use a temporary file and backup replacement so an interrupted overwrite does not silently destroy the previous preset.

Presets contain sound settings only. They exclude Bypass, Apply Auto-Gain, meter values, temporary Auto-Gain correction, A/B workflow state, and GUI animation state.

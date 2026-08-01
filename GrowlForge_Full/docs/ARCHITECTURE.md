# GrowlForge architecture — 2.2.0

GrowlForge 2.2 keeps the modular host/parameter/DSP/state/GUI split introduced by the 2.1 development refactor and uses it for the first complete feature release.

## Source tree

```text
src/
├── ClapEntry.cpp
├── common/
│   └── Math.h
├── parameters/
│   ├── ParameterDefinitions.h/.cpp
│   └── ParameterStore.h/.cpp
├── dsp/
│   ├── Filters.h
│   ├── GrowlForgeDSP.h/.cpp
│   ├── GateEngine.h/.cpp
│   ├── DriveStage.cpp
│   ├── DynamicsStage.cpp
│   ├── AutoGainEngine.h/.cpp
│   └── BypassController.h/.cpp
├── plugin/
│   ├── GrowlForgePlugin.h/.cpp
│   └── PluginFactory.h
├── state/
│   ├── StateManager.h/.cpp
│   ├── PresetManager.h/.cpp
│   └── WorkflowManager.h/.cpp
└── gui/
    └── GrowlForgeGUI.h/.cpp
```

## Responsibilities

### `parameters/`

`ParameterDefinitions` remains the single source of truth for stable IDs, preset keys, names, ranges, defaults, flags, and units.

- IDs 0–35 are unchanged.
- ID 36 is the new host-visible Bypass parameter.
- Bypass carries the CLAP bypass flag.
- Read-only activity parameters are not written to user presets.

`ParameterStore` owns atomic values and the GUI-to-host event queue. Enabling Auto-Gain no longer clears the user's Output setting.

### `dsp/`

`GrowlForgeDSP` owns frame-level stereo processing and coordinates the individual engines.

- `GateEngine` is stereo-linked, hysteretic, held, and click-free.
- `DriveStage` retains the 2.0.3 Drive voicing and subsonic/DC fix.
- `DynamicsStage` retains the existing Bloom, Sag, Dynamics, Texture, Attack, Resonance, and Compression behavior.
- `AutoGainEngine` implements stereo-linked, perceptually weighted Auto-Gain 2.0.
- `BypassController` keeps the wet path alive and crossfades to the raw input over 15 ms.

The wet path continues running during bypass. This preserves filter, gate, dynamics, and Auto-Gain state so reactivation does not produce a short level burst.

### `plugin/`

The CLAP layer handles events and buffer traversal, then publishes one meter snapshot containing:

- stereo input peak and RMS;
- stereo output peak and RMS;
- internal pre-ceiling peak;
- Gate reduction;
- internal clipping status.

GUI animation is derived from this snapshot and never runs in the audio thread.

### `state/`

Project state and user presets are intentionally separate.

- Project state format is version 12.
- State versions 7, 8, 9, 10, and 11 remain loadable.
- Project state includes Bypass, the current preset name, both A/B snapshots, and the active slot.
- User `.gfpreset` files use stable textual parameter keys.
- Bypass, momentary actions, meters, and the temporary Auto-Gain correction are excluded from presets.

### `WorkflowManager`

`WorkflowManager` owns two sound snapshots, the active A/B slot, and a bounded 64-step Undo/Redo history. It only stores preset-eligible sound parameters, so Bypass and transient meter/action state do not leak into comparison slots. Snapshot application uses the existing GUI-to-host parameter queue.

### `gui/`

The optimized cached GDI+ renderer remains in place. Version 2.2 adds:

- Bypass button and BYPASSED status;
- preset previous/next, browser, Load, and Save controls;
- RMS fill, peak trace, and peak-hold markers;
- internal clipping warning;
- live Gate-reduction activity;
- current preset name and dirty-state marker;
- A/B, copy, Undo, and Redo controls in the lower workflow strip;
- protected preset replacement and preset file management commands.

## Preserved contracts

- Plugin ID: `audio.growlforge.effect`
- Existing parameter IDs 0–35: unchanged
- Drive remains excluded from `×2`
- Drive DC/subsonic fix from 2.0.3: retained
- Untouched DSP scenarios remain bit-identical to 2.0.3
- State migration: versions 7–12

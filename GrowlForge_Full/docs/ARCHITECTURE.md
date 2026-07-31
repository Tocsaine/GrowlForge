# GrowlForge architecture — 2.1.0-dev

This refactor separates host integration, parameter ownership, DSP, state persistence, and GUI code without changing the 2.0.3 signal path.

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
│   └── AutoGainEngine.cpp
├── plugin/
│   ├── GrowlForgePlugin.h/.cpp
│   └── PluginFactory.h
├── state/
│   └── StateManager.h/.cpp
└── gui/
    └── GrowlForgeGUI.h/.cpp
```

## Responsibilities

### `ClapEntry.cpp`

Exports only the CLAP entry point and routes factory/global GUI lifetime calls. It contains no audio logic.

### `parameters/`

`ParameterDefinitions` is the single source of truth for stable IDs, names, ranges, defaults, flags, and units.

`ParameterStore` owns atomic parameter values and the GUI-to-host event queue. IDs 0–35 remain unchanged.

### `dsp/`

`GrowlForgeDSP` owns channel state and the processing order. The implementation is split by responsibility:

- `GateEngine` — the current gate detector/gain law;
- `DriveStage` — Drive, Grind, Fuzz, Growl, Mass, Bite, Harmonic Bias and Drive DC/subsonic protection;
- `DynamicsStage` — Bloom, Sag, Dynamics, Texture, Attack, Resonance and Compression;
- `AutoGainEngine` — the existing 2.0.3 RMS matcher and commit behavior;
- `Filters` — reusable one-pole and Drive high-pass filters.

No formula, coefficient, order of operations, oversampling count, parameter curve, or zero-value behavior was intentionally changed.

### `plugin/`

Owns the CLAP instance, event handling, audio buffer traversal, parameter extensions, factory, and live GUI peak publication. It delegates audio processing to `GrowlForgeDSP`.

### `state/`

Owns project-state serialization and migration. State format version remains 10. Loaders for versions 7, 8, 9, and 10 remain available.

### `gui/`

Owns the Win32/GDI+ editor and CLAP GUI extension. The optimized 2.0.2 rendering architecture is retained. GUI code now depends on the public plugin/parameter interfaces rather than being included inside the DSP translation unit.

## Preserved contracts

- Plugin ID: `audio.growlforge.effect`
- Parameter count: 36
- Parameter IDs: unchanged
- State format: version 10
- State migration: versions 7–10
- Drive excluded from `×2`
- Drive DC/subsonic fix from 2.0.3 retained
- Neutral default remains sample-identical bypass
- Existing GUI behavior and rendering logic retained

## Extension points for 2.1

The next features now have isolated locations:

- Auto-Gain 2.0: replace internals in `AutoGainEngine.cpp`;
- improved Gate: replace internals in `GateEngine.cpp`;
- click-free Bypass: add `BypassController` and one appended parameter ID;
- richer metering: add a dedicated meter snapshot without touching GUI input handling;
- presets: add `PresetManager` beside `StateManager`, keeping project state and user preset formats separate.

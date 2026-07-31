# GrowlForge 2.0 GUI

GrowlForge 2.0 adds a native scalable Windows interface through the CLAP GUI extension.
The audio processor, parameter IDs, ranges and state format remain compatible with 1.4.4.

## Layout

The interface is organized as one screen with five functional zones:

- **Input & Feel** — Input, Gate, Tight, Punch, Body and Mass.
- **Distortion Core** — Growl, Drive, Grind, Fuzz and Harmonic Bias.
- **Motion & Dynamics** — Bloom, Sag, Dynamics, Compression, Attack and Resonance.
- **Tone & Texture** — Bite, Presence, Air, Smooth, Texture and Focus.
- **Routing & Output** — Pre-Cab, Parallel Dry, Auto-Gain, Output and Ceiling.

Drive is the dominant control. Fuzz remains visually separate because it is a different
nonlinear process rather than the final step of a gain ladder.

## Interaction

- Drag vertically to change a knob.
- Hold **Shift** for fine adjustment.
- Double-click a control to restore its default value.
- Use the mouse wheel over a knob for stepped changes.
- The active parameter and exact value are shown in the top display.
- GUI changes emit CLAP value and gesture events for host automation.

## Meters

The GUI displays:

- stereo input and output level meters;
- Saturation Activity;
- Bloom Activity;
- Sag Activity;
- Compression Activity;
- Attack Activity;
- the live Auto-Gain correction with a Commit action.

The activity meters report actual DSP activity. They are not decorative animations.

## Scaling

The default size is 1200 × 720. The interface is vector-rendered with GDI+ and supports
host-controlled resizing while preserving a 5:3 aspect ratio. The minimum supported size
is 900 × 540.

## Platform status

The custom GUI is implemented for the CLAP Win32 window API. The portable non-Windows
build still compiles and exposes the same DSP and parameter interface, but reports the
custom GUI API as unsupported.

## Renderer architecture in 2.0.2

The Win32 renderer keeps a persistent backbuffer and a cached static layer. Panels,
labels, inactive meter segments, and knob bodies are rendered only when the editor is
created or resized. Mouse interaction invalidates only the affected control and the top
value display. The 30 Hz animation timer redraws only the live meter and activity regions
and is stopped while the editor is hidden.


## Source separation in 2.1.0-dev

The Win32 renderer now compiles as `src/gui/GrowlForgeGUI.cpp` instead of being included
inside the monolithic audio source. Rendering behavior, cache strategy, control geometry, and
CLAP GUI behavior are unchanged. The GUI reads parameter and meter data through the shared
plugin/parameter interfaces.

# GrowlForge 2.1.0

## Auto-Gain 2.0

- Stereo-linked loudness measurement.
- Subsonic energy is removed from the detector and the extreme top is de-emphasized.
- Pick transients have reduced influence on the target correction.
- The target is held during silence instead of drifting toward an extreme value.
- Downward and upward correction use different smoothing times to reduce pumping.
- Enabling Auto-Gain no longer resets Output.
- Commit adds the current correction to the existing Output value and disables Auto-Gain.

## Bypass

- New CLAP bypass parameter at ID 36.
- Fifteen-millisecond correlated-safe crossfade.
- The complete wet path continues running while bypassed.
- Auto-Gain continues measuring in bypass.
- Gate, filters, saturation envelopes, Bloom, Sag, Compression, and Attack remain warm.

## Gate

- Stereo-linked detector.
- Fast opening, slower closing, hysteresis, and a short hold period.
- Smooth gain transitions instead of hard switching.
- Strict zero influence and reset at Gate 0.

## Meters and visual feedback

- Separate RMS and peak indication.
- Peak-hold markers.
- Internal pre-ceiling clipping warning.
- Gate-reduction activity.
- Bypass status in the main display.
- Auto-Gain correction remains visible on the Commit control.

## State and presets

- Project-state format 11.
- Migration from state versions 7–10.
- Ten embedded factory presets.
- User `.gfpreset` Load/Save support.
- Previous/next controls and a complete preset popup menu.
- Preset dirty-state indication.
- Presets use stable textual keys and intentionally exclude Bypass and temporary meters/actions.

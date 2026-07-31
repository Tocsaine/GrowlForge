# GrowlForge 2.0.2 — GUI performance and layout fix

This update changes only the native Windows GUI renderer. The DSP path and state format are unchanged.

## Rendering changes

- Persistent full-window backbuffer; no bitmap/DC allocation in every `WM_PAINT`.
- Cached static layer for panels, labels, inactive meter segments, and knob bodies.
- Knob body bitmaps are cached per size/accent and rebuilt after resizing.
- Cached fonts, string formats, a solid brush, and a reusable pen.
- Partial invalidation for knobs, toggles, the value display, activity meters, and level meters.
- The animation timer paints only small live regions at 30 Hz.
- The timer starts in `guiShow()` and stops in `guiHide()`.
- External automation changes are detected and only the affected controls are invalidated.

## Layout fixes

- Bottom meter panel moved and resized to preserve a safe lower margin.
- dB labels moved upward.
- `-60` and `0` use edge-aware text alignment instead of center alignment.

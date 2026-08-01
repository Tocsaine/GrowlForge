# GrowlForge 2.2.1 — UI & Ceiling Fix

- Removed the redundant A/B badge from the top display.
- Repacked the bottom workflow controls so they no longer overlap the output meter labels.
- Reworked Ceiling into an exact dBFS output cap with a narrow 10% cubic soft knee.
- The active wet output can no longer exceed the selected Ceiling value.
- Bypass still returns the raw dry signal by design and therefore does not pass through Ceiling.

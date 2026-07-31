# GrowlForge 2.1.0-dev — architecture refactor

This development build changes project organization, not sound.

- Split the monolithic source into CLAP, parameter, DSP, state, and GUI modules.
- Isolated the current Gate and Auto-Gain implementations for upcoming replacements.
- Moved Drive and motion/dynamics processing into dedicated translation units.
- Preserved parameter IDs 0–35 and state format version 10.
- Preserved loading of state versions 7–10.
- Retained the optimized GUI renderer and Drive subsonic/DC fix.
- Added repeatable CLAP regression tools and an architecture regression report.

See `docs/ARCHITECTURE.md` and `docs/REGRESSION_2.1.0-dev.md`.

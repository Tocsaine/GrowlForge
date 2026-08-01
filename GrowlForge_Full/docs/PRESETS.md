# GrowlForge presets — 2.2.2

Open the preset menu by clicking the preset name in the top bar.

Factory presets are grouped into:

- **Pre-Amp** — GrowlForge before the amp simulator;
- **Post-Amp** — GrowlForge after the amp and cabinet;
- **Creative** — experimental and destructive combinations.

User presets appear under **User Presets**.

## Nolly reference setup

The album-inspired factory bank assumes the Archetype Nolly X Rhythm amp as a neutral metal foundation.

Recommended starting setup:

```text
Pre-Amp:  Guitar / DI -> GrowlForge -> Nolly Rhythm -> Nolly Cab
Post-Amp: Guitar / DI -> Nolly Rhythm -> Nolly Cab -> GrowlForge
```

Start with Nolly's boost, gate and doubler disabled; gain around 4–5; EQ, resonance and presence near noon; and output at unity.

These presets are tonal interpretations, not captures or exact replicas. Guitar, tuning, pickups, cabinet choice, playing and mix context still matter.

## User preset workflow

`SAVE` overwrites a user preset and opens Save As for a factory preset. The menu provides Save As, Rename, Delete, Refresh and Open Preset Folder.

User presets are stored by default in:

```text
%APPDATA%\GrowlForge\Presets
```

Preset writes use temporary and backup files. Presets exclude Bypass, activity meters, temporary Auto-Gain correction and A/B workflow state.

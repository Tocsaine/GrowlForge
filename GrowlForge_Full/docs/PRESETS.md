# GrowlForge presets

## Factory bank

GrowlForge 2.1 contains ten factory presets embedded in the plugin:

- Init
- Controlled Fuzz
- Modern Rhythm
- Palm Weight
- Post Amp Bite
- Synth Growl
- Crushed Bloom
- Parallel Attack
- Wide Open
- Color x2

Use the left/right arrows to cycle presets. Click the preset name to open the complete preset menu.

## User presets

The GUI provides `LOAD` and `SAVE` buttons. User presets use the `.gfpreset` extension and are stored by default in:

```text
%APPDATA%\GrowlForge\Presets
```

A preset is a readable JSON document with stable parameter keys. Presets contain sound settings only. They do not contain:

- Bypass;
- momentary Apply Auto-Gain state;
- meter values;
- the temporary Auto-Gain correction;
- GUI hover or animation state.

The current preset name is shown in the top display. An asterisk means the sound has changed since that preset was loaded or saved.

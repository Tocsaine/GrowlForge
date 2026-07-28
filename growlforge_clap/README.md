# GrowlForge CLAP

GrowlForge is a lightweight stereo guitar distortion/fuzz effect aimed at tight modern metal and metalcore rhythm tones. It is an original processor, not an exact model of any band's proprietary recording chain.

## Signal chain

1. Input trim
2. Threshold gate
3. Variable high-pass tightener
4. Low-mid growl resonance
5. Asymmetric soft clipping blended with restrained fuzz
6. Presence shaping
7. Cabinet-style low-pass filter
8. Wet/dry mix and output trim

## Parameters

- **Input**: gain before distortion.
- **Drive**: primary saturation amount.
- **Fuzz**: rougher, grainier clipping component.
- **Growl**: low-mid resonance and asymmetry.
- **Tight**: removes low-frequency flub before clipping.
- **Presence**: upper-mid attack around 2.9 kHz.
- **Cab Filter**: lowers the high-frequency cutoff; higher values sound darker.
- **Gate**: input threshold in dB.
- **Mix**: wet/dry blend.
- **Output**: final level.

## Recommended starting preset

For a Bodies-inspired modern metal texture:

- Input: 0 dB
- Drive: 4.8
- Fuzz: 3.2
- Growl: 6.3
- Tight: 6.8
- Presence: 5.4
- Cab Filter: 7.0
- Gate: -58 dB
- Mix: 100%
- Output: -5 dB

Use a clean DI guitar signal. For best results, place an IR loader after GrowlForge and reduce **Cab Filter** to around 2-4, or use GrowlForge alone with **Cab Filter** around 6-8.

## Build

### Windows (Visual Studio 2022)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The resulting file is usually:

`build/Release/GrowlForge.clap`

Copy it to:

`%LOCALAPPDATA%\Programs\Common\CLAP\`

or another CLAP directory scanned by REAPER, then run **Preferences > Plug-ins > VST > Re-scan**.

### Linux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
mkdir -p ~/.clap
cp build/GrowlForge.clap ~/.clap/
```

### macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
mkdir -p ~/Library/Audio/Plug-Ins/CLAP
cp -R build/GrowlForge.clap ~/Library/Audio/Plug-Ins/CLAP/
```

## Notes

- No custom GUI yet; REAPER displays the parameters through its generic interface.
- The processor currently relies on host oversampling. In REAPER, try 2x or 4x per-FX oversampling for cleaner high-frequency behavior.
- Input level matters. Peaks around -12 to -6 dBFS are a practical starting range.

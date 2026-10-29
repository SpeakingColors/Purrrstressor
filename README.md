# Purrrstressor

A free, open source, cat-themed compressor plugin (VST3) inspired by the behaviour of the Empirical Labs Distressor. Our own DSP, written from the public manuals. Not affiliated with or endorsed by Empirical Labs or Universal Audio.

Status: early. DSP core and plugin shell only; the real interface comes next.

## What it does

- Ratios 1:1, 2:1, 3:1, 4:1, 6:1, 10:1 (Opto), 20:1 and Nuke, each with its own knee
- Detector: high-pass (100 Hz), band emphasis (about 6 kHz), stereo link
- Audio: 18 dB/oct Bessel high-pass, Dist 2 (2nd harmonic), Dist 3 (3rd harmonic)
- Input, Attack (50 us to 30 ms), Release (50 ms to 3.5 s), Output, Headroom, Mix

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Core tests (no JUCE needed): `sh tests/run.sh`

## Privacy

Collects nothing. No network access.

## Licence

AGPL-3.0-or-later (JUCE is used under AGPLv3).

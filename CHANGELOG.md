# SubHarm changelog

## 0.5.0
- New interface ("neon studio"): top bar with key display, BPM/TAKE LCD readouts and side chain meter; full-width song timeline; LINES tokens in one colour per line; PERFORMANCE; OUTPUT with neon ring knobs; STACKS with MIX/STEMS drag pads; status bar
- Fonts: Manrope (interface) and DSEG7 (LCD values), embedded (OFL)
- Timeline renders from a cache: smooth playhead, lower CPU
- CI: pluginval (strictness 5) on every build; macOS builds on release tags

## 0.4.0
- Revoice-style doubles: own noise seed per voice (no more phasing), per-note timing and pitch, scoop, drift, vibrato depth, level ride, formant
- UNIS (pedal on the key's tonic), UNIS 5TH (pedal on the 5th), DOUBLE; default stack UNIS + 3RD UP + OCT UP + OCT DOWN
- Standalone app; Windows .exe installer (VST3 + app); macOS .pkg (VST3 + AU + app)

## 0.3.0
- Hardware-style interface

## 0.2.0
- Key selector (auto or set by hand), named L/R line matrix, in-key harmonies, fewer jumps, stems named by line

## 0.1.0
- First version: chord/key detection from the side chain, harmony stacks (R&B/Pop/Clean/Gospel), humanize, MIX/STEMS drag to DAW

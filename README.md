# FreeLA2 v0.3.1

An optical leveling amplifier modelled on Chris's Acustica OPTO-3A/2A Nebula set (the LA-2A model):
**PREAMP > LEVELING AMPLIFIER > CLIPPER** in one plug-in, fitted to 219 captures at 96 kHz.
VST3 + AU, universal macOS (Apple Silicon + Intel). The top bar shows the version and the build number.

## Controls
**Top bar**
- **FILTER** OFF / CLEAN / AIR - ultrasonic band-limiting (works when the plug-in runs at 88.2 kHz or more inside:
  any oversampling setting, or Off in an 88.2-192 kHz session).
  - OFF: none. Use it for null tests against the Nebula.
  - CLEAN (default): a complete 25 kHz filter (Airwindows Hypersonic, 14th-order Butterworth) after each stage.
    Flat to 20 kHz; every stage sees a flat top end.
  - AIR: an 8th-order 25 kHz filter split in four (D > Pre > C > Leveler > B > Clipper > A), then a complete
    Hypersonic. The stages are driven with a softer top, the resonant A stage brings the air back on the way out
    (the Clipper's harmonics come out up to +6 dB brighter at 20 kHz); the clean signal stays flat to 20 kHz.
- **ANALOG / LINEAR** (next to FILTER): the filters' phase.
  - ANALOG (default): analog-style filters. The top octave lags slightly (about -18 deg at 10 kHz, -65 deg at 15 kHz
    with 2x CLEAN; AIR about half that). Lowest latency.
  - LINEAR: a correction filter at the output removes the phase of the ultrasonic filters and the oversampling
    filters; same level response (within 0.01 dB), +1 ms latency. Use it when FreeLA2 is blended with an unprocessed
    copy of the same track (parallel bus, multi-mic) or for phase-critical work.
  In both, the low and mid frequencies line up exactly with the reported latency in every OVERSAMPLE and FILTER setting.
- **OVERSAMPLE** Off / 2x / 4x / 8x (8x up to 96 kHz sessions, 4x above). **RENDER** AUTO / ALWAYS / NEVER and its
  factor (2x / 4x / 8x): the oversampling used when REAPER renders.
- **Power**: clean bypass (latency-matched).

**PREAMP**
- **PUSHED / NORMAL**: the two Nebula programs. Same level and EQ; PUSHED keeps more 2nd harmonic at moderate levels.
- **DRIVE** -30 ... +30: only the 2nd harmonic (dB). Level stays put.
- **OUTPUT** -18 ... +18 dB = the Leveler's input. One gain for both: the Nebula's Pre OUT and Comp INPUT sit back to
  back with nothing but a linear filter between them, so one knob does exactly what the two did. More = more
  compression and more level into the Clipper (not compensated). 0 dB = the Nebula as loaded.

**LEVELING AMPLIFIER**
- **LIMIT / COMP**: COMP is about 5-7:1 above a 15 dB soft knee, LIMIT holds the output flat.
- **PEAK REDUCTION** 0-100 (default 0 = no compression, like the Nebula as loaded): the threshold (about 57 dB over
  the knob). 50 = about 6 dB of reduction at -10 dBFS.
- **GAIN** 0-25 dB make-up.
- **AHEAD** 0 / 1.5 / 3 / 4.5 / 6 ms: a real look-ahead (adds that much latency). 0 = the original.
- **EMPHASIS**: left = FAST ATTACK (the detector hears all frequencies alike), right = SLOW BASS (the detector
  hears less bass and reacts to it more slowly, so kick and bass punch through). Centre = the Nebula default.
- **MIX** 0-100 %: blends the Leveler only (parallel compression; replaces the Nebula DRY knob). 0 % = the Preamp
  goes straight into the Clipper. The dry part is time-aligned (also with AHEAD); GAIN acts on the compressed part.
- **METER** GR / OUT (0 VU = -18 dBFS RMS), **ON** lamp.

**CLIPPER**
- **DRIVE** 0-40: clipping starts at about (+2 - DRIVE) dBFS; the level is compensated below the onset.
- **OUTPUT** 0-10 dB. **CLIP** lamp lights while it clips.

## Changes in v0.3.1
- Fix: the plug-in now loads on macOS 11 (Big Sur) and later, Intel and Apple Silicon. v0.1-v0.3 were built for the
  GitHub build machine's macOS (26.5) by mistake, so older Macs refused them. The build now checks this itself.

## Changes in v0.3
- FILTER phase: ANALOG / LINEAR (`Source/PhaseAlign.h`, reusable).
- Exact alignment: the latency now comes from the measured delay of the oversampling and filter chain, with a
  fractional-delay correction at 88.2 kHz and up (v0.2 was up to 36 us early with OVERSAMPLE Off and a filter on).
  Latency at 96 kHz: 32 samples (ANALOG), 120 samples (LINEAR), the same in every OVERSAMPLE and FILTER setting.

## Changes in v0.2
- PEAK REDUCTION defaults to 0. Choosing a finish repaints the whole faceplate.
- Pre OUTPUT and Leveler INPUT merged into one knob (Pre OUTPUT, -18 ... +18 dB). MIX blends the Leveler only.
- Tested against all 219 Nebula renders with the plug-in itself (`docs/test_v0.2_vs_nebula.txt`), and fixed what
  that turned up: the 0.1 / 0.25 dB loss at PEAK REDUCTION 0, short gain-reduction spikes on drum hits and note
  onsets at stronger settings, and the Leveler timing (refitted on drums, bass and vocal as well as the bursts).

## What was fitted (against the Nebula renders at 96 kHz)
- Pre: EQ in level and phase (sub-bass bump, roll-off 8 Hz-24 kHz), 2nd/3rd harmonic vs level, DRIVE and the
  NORMAL/PUSHED difference. Nulls -33 to -40 dB on its own.
- Leveler: static curves (2-D table over level and PEAK REDUCTION, 0.15-0.4 dB), opto timing (program-dependent
  attack, release speed that depends on the gain reduction, slow memory): 0.23-0.34 dB rms over all the burst
  tests; EMPHASIS; the cell's odd harmonics (about 4 dB rms); stereo on L+R.
- Clipper: curve and harmonics (about 0.3 dB / 1.5 dB), oversampled here (the Nebula one aliases).
- Whole chain as loaded nulls about -29 dB (median; -21 to -32); with 8 dB of compression about -18 dB and the level
  within about 0.6 dB rms. Leveler alone at PEAK REDUCTION 0: -42 to -50 dB. Program material (drums, bass, vocal):
  gain-reduction error 0.7-1.6 dB rms, the vocal within about 0.5 dB. Full results: `docs/test_v0.2_vs_nebula.txt`.

## Known limits of v0.3 (unchanged from v0.2)
- The Pre's 2nd harmonic has a different phase from the Nebula's (same level), so nulls at high DRIVE are limited
  (about -20 dB at DRIVE +15, -3 to -7 dB at +30). The Nebula also leaves a short low-frequency tail after loud
  passages at high DRIVE (250 ms, -28 dB at full DRIVE); FreeLA2 does not.
- NORMAL's low-level 2nd harmonic at 100 Hz is up to 8 dB higher than the Nebula's (around -80 dBc).
- Leveler: on long holds it reaches its final gain reduction a little sooner; full-scale tones from silence are
  grabbed harder at first; on drums at strong settings a bit more on kick-like hits, 2-3 dB less on snare-like hits.
- Clipper waveform at heavy DRIVE (level within about 0.5-1 dB).

## Latency
Fixed in every OVERSAMPLE and FILTER setting: 32 samples at 96 kHz in ANALOG, 120 in LINEAR, plus AHEAD when used.
Reported to the host.

## Files
- `Source/LA2Engine.h` - the DSP engine (no JUCE). `Source/FreeBandLimit.h` - the reusable ultrasonic filter module
  (OFF / CLEAN / AIR, 4, 5 or 7 stages) for Chris's other plug-ins. `Source/PhaseAlign.h` - the reusable alignment /
  linear-phase correction (measure the linear chain's impulse response, then ANALOG or LINEAR). `Source/ModelData.h` - generated from
  `docs/freela2_model_v0.json` by `tools/make_model_data.py`.
- `Source/PluginProcessor.*`, `Source/PluginEditor.*` - the plug-in and GUI.
- `tests/plugin_test.cpp` - local test (cmake -DFLA2_BUILD_TESTS=ON). `tests/compare_nebula.cpp` + `fit/compare_nebula.py`
  - runs the plug-in over the Nebula capture signals and compares. `fit/` - the Python fitting scripts (reference).
- `docs/test_v0.3.txt` - plug-in test results; `docs/test_v0.3_vs_nebula.txt` and `docs/test_v0.2_vs_nebula.txt` -
  the comparisons with the Nebula (the v0.2 file has every render listed); `docs/gui_v0.3.png` - GUI snapshot.

## Credits
- Ultrasonic filter sections: Airwindows Hypersonic / HypersonX / UltrasonX by Chris Johnson, MIT licence (`docs/airwindows-LICENSE.txt`).
- JUCE 8.0.4 (juce.com) under its own licence.

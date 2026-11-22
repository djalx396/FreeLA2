# FreeLA2 v0.1

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
- **OVERSAMPLE** Off / 2x / 4x / 8x (8x up to 96 kHz sessions, 4x above). **RENDER** AUTO / ALWAYS / NEVER and its
  factor (2x / 4x / 8x): the oversampling used when REAPER renders.
- **Power**: clean bypass (latency-matched).

**PREAMP**
- **PUSHED / NORMAL**: the two Nebula programs. Same level and EQ; PUSHED keeps more 2nd harmonic at moderate levels.
- **DRIVE** -30 ... +30: only the 2nd harmonic (dB). Level stays put.
- **OUTPUT** -12 ... +6 dB.

**LEVELING AMPLIFIER**
- **LIMIT / COMP**: COMP is about 5-7:1 above a 15 dB soft knee, LIMIT holds the output flat.
- **PEAK REDUCTION** 0-100: the threshold (about 57 dB over the knob). 50 = about 6 dB of reduction at -10 dBFS.
- **GAIN** 0-25 dB make-up.
- **INPUT** -12 ... +12 dB into the leveler (not compensated: more input = more compression and more level).
- **AHEAD** 0 / 1.5 / 3 / 4.5 / 6 ms: a real look-ahead (adds that much latency). 0 = the original.
- **EMPHASIS**: left = FAST ATTACK (the detector hears all frequencies alike), right = SLOW BASS (the detector
  hears less bass and reacts to it more slowly, so kick and bass punch through). Centre = the Nebula default.
- **MIX** 0-100 % dry/wet of the whole plug-in (replaces the Nebula DRY knob).
- **METER** GR / OUT (0 VU = -18 dBFS RMS), **ON** lamp.

**CLIPPER**
- **DRIVE** 0-40: clipping starts at about (+2 - DRIVE) dBFS; the level is compensated below the onset.
- **OUTPUT** 0-10 dB. **CLIP** lamp lights while it clips.

## What was fitted (v0.1, against the Nebula renders at 96 kHz)
- Pre: EQ in level and phase (sub-bass bump, roll-off 8 Hz-24 kHz), 2nd/3rd harmonic vs level, DRIVE and the
  NORMAL/PUSHED difference. Nulls -33 to -40 dB on its own.
- Leveler: static curves (2-D table over level and PEAK REDUCTION, 0.15-0.4 dB), opto timing (program-dependent
  attack, release speed that depends on the gain reduction, slow memory): 0.23-0.34 dB rms over all the burst
  tests; EMPHASIS; the cell's odd harmonics (about 4 dB rms); stereo on L+R.
- Clipper: curve and harmonics (about 0.3 dB / 1.5 dB), oversampled here (the Nebula one aliases).
- Whole chain as loaded nulls about -32 dB; with compression about -17 dB (small differences in the moment-to-moment gain).

## Known limits of v0.1
- NORMAL's low-level 2nd harmonic at 100 Hz is up to 8 dB higher than the Nebula's (around -80 dBc).
- The Pre's 2nd harmonic has a different phase from the Nebula's (same level), so nulls at high DRIVE are limited.
- The leveler's very first milliseconds of attack at moderate settings are a bit fast; the release between 0.1 and
  1 s is a bit quick after medium gain reduction.

## Latency
Fixed in every OVERSAMPLE and FILTER setting (23 samples at 96 kHz), plus AHEAD when used. Reported to the host.

## Files
- `Source/LA2Engine.h` - the DSP engine (no JUCE). `Source/FreeBandLimit.h` - the reusable ultrasonic filter module
  (OFF / CLEAN / AIR, 4, 5 or 7 stages) for Chris's other plug-ins. `Source/ModelData.h` - generated from
  `docs/freela2_model_v0.json` by `tools/make_model_data.py`.
- `Source/PluginProcessor.*`, `Source/PluginEditor.*` - the plug-in and GUI.
- `tests/plugin_test.cpp` - local test (cmake -DFLA2_BUILD_TESTS=ON). `fit/` - the Python fitting scripts (reference).
- `docs/test_v0.1.txt` - test results; `docs/gui_v0.1.png` - GUI snapshot.

## Credits
- Ultrasonic filter sections: Airwindows Hypersonic / HypersonX / UltrasonX by Chris Johnson, MIT licence (`docs/airwindows-LICENSE.txt`).
- JUCE 8.0.4 (juce.com) under its own licence.

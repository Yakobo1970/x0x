# x0x MIDI-Steuerung

## MODE: RACK ("PATTERNS TO DAW")

Noten 36-67 (C1-G3, C3 = 60) auf Kanal 1-4 wählen Pattern A1-D8. Velocity >= 100 wechselt sofort und startet das Pattern neu, kleinere Velocity wechselt am Pattern-Ende.

## MODE: BASS 1 ... DRUM 09 (Single Mode, "NOTES TO DAW")

Nur das gewählte Gerät klingt, gespielt von MIDI-Noten auf beliebigem Kanal. CCs auf Kanal 5 steuern Mixer + FX, CCs auf jedem anderen Kanal das Gerät (Nummern wie unten).

- **303:** Note 36 (C1) = Pattern-Note C, Oktave 0. Überlappende Noten gleiten (Slide), Velocity >= 100 = Accent.
- **Drums (GM):** BD 36, RS 37, SD 38, CP 39, LT 41, CH 42, MT 45, OH 46, HT 48, CY/Crash 49, Ride 51, Cowbell 56. Velocity < 64 = Flam-Vorschlag (halbe Lautstärke), 64-99 normal, 100-119 = 909-Einzelaccent (808: Accent), 120-126 = Accent, 127 = Accent + Einzelaccent.

## Kanal 1 - BASS 1

| CC | Parameter |
|---:|---|
| 3 | Bass 1 Tuning (`bass1.tuning`) |
| 4 | Bass 1 Cutoff (`bass1.cutoff`) |
| 5 | Bass 1 Resonance (`bass1.resonance`) |
| 8 | Bass 1 Env Mod (`bass1.envmod`) |
| 9 | Bass 1 Decay (`bass1.decay`) |
| 12 | Bass 1 Accent (`bass1.accent`) |
| 13 | Bass 1 Volume (`bass1.volume`) |
| 14 | Bass 1 Square Wave (`bass1.waveform`) |
| 15 | Bass 1 On (`bass1.on`) |

## Kanal 2 - BASS 2

| CC | Parameter |
|---:|---|
| 3 | Bass 2 Tuning (`bass2.tuning`) |
| 4 | Bass 2 Cutoff (`bass2.cutoff`) |
| 5 | Bass 2 Resonance (`bass2.resonance`) |
| 8 | Bass 2 Env Mod (`bass2.envmod`) |
| 9 | Bass 2 Decay (`bass2.decay`) |
| 12 | Bass 2 Accent (`bass2.accent`) |
| 13 | Bass 2 Volume (`bass2.volume`) |
| 14 | Bass 2 Square Wave (`bass2.waveform`) |
| 15 | Bass 2 On (`bass2.on`) |

## Kanal 3 - DRUM 08

| CC | Parameter |
|---:|---|
| 3 | 808 BD Level (`r808.bd.level`) |
| 4 | 808 BD Tone (`r808.bd.tone`) |
| 5 | 808 BD Decay (`r808.bd.decay`) |
| 8 | 808 SD Level (`r808.sd.level`) |
| 9 | 808 SD Tone (`r808.sd.tone`) |
| 12 | 808 SD Snappy (`r808.sd.snappy`) |
| 13 | 808 LT Level (`r808.lt.level`) |
| 14 | 808 LT Tuning (`r808.lt.tuning`) |
| 15 | 808 MT Level (`r808.mt.level`) |
| 16 | 808 MT Tuning (`r808.mt.tuning`) |
| 17 | 808 HT Level (`r808.ht.level`) |
| 18 | 808 HT Tuning (`r808.ht.tuning`) |
| 19 | 808 RS Level (`r808.rs.level`) |
| 20 | 808 CP Level (`r808.cp.level`) |
| 21 | 808 CB Level (`r808.cb.level`) |
| 22 | 808 CY Level (`r808.cy.level`) |
| 23 | 808 CY Tone (`r808.cy.tone`) |
| 24 | 808 CY Decay (`r808.cy.decay`) |
| 25 | 808 OH Level (`r808.oh.level`) |
| 26 | 808 OH Decay (`r808.oh.decay`) |
| 27 | 808 CH Level (`r808.ch.level`) |
| 28 | 808 Accent Level (`r808.ac.level`) |
| 29 | 808 LT/LC (`r808.lt.alt`) |
| 30 | 808 MT/MC (`r808.mt.alt`) |
| 31 | 808 HT/HC (`r808.ht.alt`) |
| 33 | 808 RS/CL (`r808.rs.alt`) |
| 34 | 808 CP/MA (`r808.cp.alt`) |
| 35 | 808 On (`r808.on`) |

## Kanal 4 - DRUM 09

| CC | Parameter |
|---:|---|
| 3 | 909 BD Tune (`r909.bd.tune`) |
| 4 | 909 BD Level (`r909.bd.level`) |
| 5 | 909 BD Attack (`r909.bd.attack`) |
| 8 | 909 BD Decay (`r909.bd.decay`) |
| 9 | 909 SD Tune (`r909.sd.tune`) |
| 12 | 909 SD Tone (`r909.sd.tone`) |
| 13 | 909 SD Snappy (`r909.sd.snappy`) |
| 14 | 909 SD Level (`r909.sd.level`) |
| 15 | 909 LT Tune (`r909.lt.tune`) |
| 16 | 909 LT Decay (`r909.lt.decay`) |
| 17 | 909 LT Level (`r909.lt.level`) |
| 18 | 909 MT Tune (`r909.mt.tune`) |
| 19 | 909 MT Decay (`r909.mt.decay`) |
| 20 | 909 MT Level (`r909.mt.level`) |
| 21 | 909 HT Tune (`r909.ht.tune`) |
| 22 | 909 HT Decay (`r909.ht.decay`) |
| 23 | 909 HT Level (`r909.ht.level`) |
| 24 | 909 Rim Level (`r909.rs.level`) |
| 25 | 909 Clap Level (`r909.cp.level`) |
| 26 | 909 HH Level (`r909.hh.level`) |
| 27 | 909 CH Decay (`r909.hh.chDecay`) |
| 28 | 909 OH Decay (`r909.hh.ohDecay`) |
| 29 | 909 Crash Level (`r909.cy.crLevel`) |
| 30 | 909 Crash Tune (`r909.cy.crTune`) |
| 31 | 909 Ride Level (`r909.cy.rdLevel`) |
| 33 | 909 Ride Tune (`r909.cy.rdTune`) |
| 34 | 909 Accent Level (`r909.ac.level`) |
| 35 | 909 Flam (`r909.ac.flam`) |
| 36 | 909 On (`r909.on`) |

## Kanal 5 - MIXER + FX

| CC | Parameter |
|---:|---|
| 3 | Bass 1 Pan (`mixer.bass1.pan`) |
| 4 | Bass 1 Delay Send (`mixer.bass1.delay`) |
| 5 | Bass 1 Level (`mixer.bass1.level`) |
| 8 | Bass 1 Mute (`mixer.bass1.mute`) |
| 9 | Bass 1 Solo (`mixer.bass1.solo`) |
| 12 | Bass 1 Dist Insert (`mixer.bass1.dist`) |
| 13 | Bass 2 Pan (`mixer.bass2.pan`) |
| 14 | Bass 2 Delay Send (`mixer.bass2.delay`) |
| 15 | Bass 2 Level (`mixer.bass2.level`) |
| 16 | Bass 2 Mute (`mixer.bass2.mute`) |
| 17 | Bass 2 Solo (`mixer.bass2.solo`) |
| 18 | Bass 2 Dist Insert (`mixer.bass2.dist`) |
| 19 | 808 Pan (`mixer.r808.pan`) |
| 20 | 808 Delay Send (`mixer.r808.delay`) |
| 21 | 808 Level (`mixer.r808.level`) |
| 22 | 808 Mute (`mixer.r808.mute`) |
| 23 | 808 Solo (`mixer.r808.solo`) |
| 24 | 808 Dist Insert (`mixer.r808.dist`) |
| 25 | 909 Pan (`mixer.r909.pan`) |
| 26 | 909 Delay Send (`mixer.r909.delay`) |
| 27 | 909 Level (`mixer.r909.level`) |
| 28 | 909 Mute (`mixer.r909.mute`) |
| 29 | 909 Solo (`mixer.r909.solo`) |
| 30 | 909 Dist Insert (`mixer.r909.dist`) |
| 31 | Delay Return (`mixer.delayReturn`) |
| 33 | Master Fader (`mixer.master.level`) |
| 34 | Master Volume (`master.volume`) |
| 35 | Shuffle (`song.shuffle`) |
| 36 | Delay On (`fx.delay.on`) |
| 37 | Delay Steps (`fx.delay.steps`) |
| 39 | Delay Feedback (`fx.delay.feedback`) |
| 40 | Delay Pan (`fx.delay.pan`) |
| 41 | Delay Triplet (`fx.delay.triplet`) |
| 42 | Dist On (`fx.dist.on`) |
| 43 | Dist Amount (`fx.dist.amount`) |
| 44 | Dist Shape (`fx.dist.shape`) |
| 45 | Comp On (`fx.comp.on`) |
| 46 | Comp Amount (`fx.comp.amount`) |
| 47 | Comp Threshold (`fx.comp.threshold`) |
| 48 | Comp Target (`fx.comp.target`) |
| 49 | Comp Routed (`fx.comp.routed`) |
| 50 | PCF On (`fx.pcf.on`) |
| 51 | PCF Freq (`fx.pcf.freq`) |
| 52 | PCF Reso (`fx.pcf.reso`) |
| 53 | PCF Amount (`fx.pcf.amount`) |
| 54 | PCF Decay (`fx.pcf.decay`) |
| 55 | PCF Wave (`fx.pcf.wave`) |
| 56 | PCF Mode (`fx.pcf.mode`) |
| 57 | PCF Target (`fx.pcf.target`) |

# EPN DR-16 — Digital Reverberation System

VST3 / AU / Standalone emulation of the 1982 AMS RMX16 digital reverb, by EPN Audio.
Mono in, stereo out, 18 programs (the 9 factory ones plus the 9 from the remote terminal),
calculator-style front panel operated the way the original manual describes.

## Build

**Windows / macOS (CI):** push to GitHub, the workflow in `.github/workflows/build.yml`
builds VST3 + Standalone (+ AU on macOS, universal binary) and runs the DSP tests.
Artefacts appear on the Actions run page.

**Local:**
```
cmake -B build -DCMAKE_BUILD_TYPE=Release          # fetches JUCE 8.0.4
cmake --build build --config Release
```
Use `-DJUCE_DIR=/path/to/JUCE` to build against a local checkout.

## Front panel

| Control | What it does |
|---|---|
| PROGRAM / PRE / DECAY / LO / HI | select the parameter. LED on = selected, flashing = keypad entry in progress |
| MIX / IN / OUT | dry/wet %, input and output level (dB) |
| keypad | type a value, `#` = enter. `*` = decimal point (decay, mix) or minus sign (filters, levels). Out-of-range entry shows **E**; press the function key again to clear |
| A / B | in ECHO selects which output PRE/DECAY program (ECHO A max 1.2 s, ECHO B max 400 ms). C and D do nothing, as on the original |
| DOWN / UP and the DATA encoder | nudge the selected value (pre 10 ms, chorus 1 ms, decay 0.1 s, filters 1). Shift-drag on the encoder = fine |
| SAVE / RECALL | 100 user memories (00–99), stored in the session |
| 16-BIT | on = gain-ranged ADC, 16-bit processing word, 16-bit DAC. Off = same algorithms, no quantisation |

Computer keyboard works too: digits, `.`/`-` = `*`, Enter = `#`, arrows = nudge, A/B.

Changing program loads that program's defaults and mutes the output for 250 ms
("memory flush-out"), exactly like the hardware.

## Programs

| # | Name | Filters | Notes |
|---|---|---|---|
| 1 | AMBIENCE | lo/hi | between hall and plate, no early reflections. **9.9 with both filters at 0 = infinite, lossless decay** (freeze trick) |
| 2 | ROOM A1 | hi | live room, high colouration, top end restrained. Best at 1–2 s, hi −7/−8 |
| 3 | HALL C1 | lo/hi | strong early reflections, high presence (vocals) |
| 4 | PLATE A1 | lo/hi | high diffusion, even decay |
| 5 | HALL B3 | lo/hi | as C1, lower-energy early reflections |
| 6 | CHORUS | hi | 5 voices panned across the image. PRE = voice separation (0–50 ms, ×5 spread), DECAY = randomising depth |
| 7 | ECHO | lo/hi | two independent delays with regeneration and filters in the loop |
| 8 | NONLIN 2 | hi | stereo gated: near-flat for the decay time, then drops fast |
| 9 | REVERSE 1 | — | build-up then sudden stop. L dispersed, R discrete |
| 10 | REVERSE 2 | hi | as above with HF control |
| 11 | FREEZE | — | decay 0.0 = clear, 0.1 = hold & mute input, ≥0.2 = hold & add |
| 12 | ROOM A0 | hi | original ROOM A1, unrestrained top |
| 13 | ROOM B1 | hi | constant diffusion; DECAY (max 0.7 s) also scales the room size |
| 14 | HALL A1 | hi | earlier, less smooth hall |
| 15 | PLATE B1 | lo/hi | alternative plate |
| 16 | DELAY | — | up to 810 ms on output 2 only; output 1 = straight through the converters |
| 17 | IMAGE P1 | hi | nonlin that pans L→R as it proceeds (few ms to 0.6 s) |
| 18 | NONLIN 1 | — | flat then gone. L diffused, R discrete |

Filter law (manual, Appendix C): HI 0…−9 sets the frequency where decay time is **half** the
indicated value (18k, 15k, 10k, 8k, 6.5k, 5k, 3.5k, 2k, 1.5k, 1k). LO −9…+9 scales the
decay at 350 Hz from ¼× to 4×.

## What is documented and what is reconstructed

**Taken from documentation**
- Program list, controls, ranges, filter laws, keypad behaviour, freeze/echo/delay logic:
  RMX16 user manual (original text, reprinted by AMS Neve / UA for the Expanded version).
- Converter architecture: AMS service schematics of the same generation (DMX 15-80S):
  S/H + successive-approximation ADC with gain ranging, PCM53-class 16-bit DAC with deglitch
  hold, multi-stage active anti-alias and reconstruction filters.
- Overall A/D/A response tuned to the AMS spec: 20 Hz–18 kHz, −3 dB / +0 dB
  (measured on this build: −0.6 dB @ 15 kHz, −3.0 dB @ 18 kHz).

**Reconstructed (not public)**
- The actual reverb microcode lived in EPROM and has never been published. Topologies here
  (FDN with Hadamard mixing, diffusion, early-reflection patterns, tapped nonlin/reverse
  generators) are built from AMS's descriptions of each program. Character, controls and
  calibration follow the manual; the sample-for-sample sound will not match a real unit.
- Internal sample rate: 40 kHz (not officially documented).
- Max pre-delay for HALL C1, NONLIN 1/2, REVERSE 1/2, ROOM B1, IMAGE P1: manual gives no figure, set to 200 ms.
- Gate length per decay value on NONLIN/REVERSE ("values are not calibrated" per manual): 0.1 s per unit.

The best next step for fidelity is an impulse-response comparison against a real RMX16 or
the AMS Neve 500 module (which runs the original code): record clicks through each program
at a few decay settings and tune `Engines.h` voicings to match.

## Verification (`dr16_tests`)

- RT60 measured at 1 kHz within ±4% of indicated on all reverb programs (1, 2.5, 5 s).
- FREEZE / AMBIENCE 9.9: −0.9 dB after 25 s of hold (lossless loop; overflow lights when overdriven, as the manual warns).
- All 18 programs finite at 44.1 / 88.2 / 96 / 192 kHz host rates.
- CPU: 0.3–2.8% of one core per instance at 48 kHz.

## Legal

EPN DR-16 is an independent emulation. AMS, RMX16 and AMS Neve are trademarks of their
respective owner; no affiliation or endorsement. No AMS code, artwork or logos are used.

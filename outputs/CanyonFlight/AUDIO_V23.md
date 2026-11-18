# CirrusFlight v23 — flight audio

V23 adds a small procedural sound engine:

- SR22 piston/propeller harmonics, with estimated pitch and level following the lagged engine-power state.
- SF50 turbine whine, low rumble and filtered noise, with pitch/level following engine power.
- Airspeed-dependent wind noise. Exterior camera sound is louder than the cockpit mix.
- A common 850-Hz stall-warning tone when the existing stall-warning value exceeds 0.5.

These sounds are synthesized, not recordings of an IO-550 or FJ33. Audio pitch is not an RPM/N1 instrument and does not add engine/governor fidelity to the flight model. The common stall tone is a simulator cue, not a reproduction of either aircraft's certified warning system or the SF50 stick shaker. The three save slots and flight dynamics remain unchanged.

## Controls

Choose Audio from either the title menu or START / MENU while flying. Volume has six levels, 0–100% in 20% steps; Sound toggles mute. Left/right adjusts the selected row, A cycles/toggles, and B/Back returns. Startup volume is 60%; these preferences last for the current app session and are not stored in flight saves. Hardware volume remains available independently.

Sound fades to silence on pause, menus, crash and mute. Engine cutoff fades the engine voice while retaining wind noise. Changes in engine state and aircraft voice are smoothed, and oscillator phases carry across PCM blocks. There is no music, radio/ATC, recorded cockpit speech or positional surround audio in this release.

## Playback and resources

The backend uses libctru NDSP, one mono PCM16 channel centered in both outputs, 22,050 samples/second and three 512-sample buffers (3 KiB). A dedicated worker fills only free/completed buffers and polls every 4 ms; it does no SD reads or writes during playback. The nominal queue holds about 70 ms of audio, with additional polling/envelope and device latency. Synthesis uses a 1024-entry sine lookup table, noise filtering and smooth gain changes rather than per-sample trigonometric calls. The worker stack is 16 KiB, separate from libctru's own DSP resources. No measured device CPU/frame-rate result is claimed.

The worker stops and joins before NDSP shutdown, and PCM memory is freed after the DSP is stopped. Audio initialization failure leaves the simulator usable; the Audio page shows **Audio unavailable on this system**. NDSP needs a working DSP component supplied by the launcher or the existing `/3ds/dspfirm.cdc` setup, as documented in [libctru's initialization implementation](https://github.com/devkitPro/libctru/blob/master/libctru/source/ndsp/ndsp.c). This release includes original sound synthesis only and does not bundle DSP firmware.

## Verification and install

6,167 compiled ARM synthesis checks passed: continuous output across different block sizes, nonzero signals, exact silence after mute/pause decay, output-buffer bounds and unclipped output including maximum-volume combinations. FFT checks confirm different idle/power pitch behavior and the stall-tone frequency. Actual PCM produced by that ARM code is exported as six two-second WAV previews under `art/audio/` (SR22 idle/power, SF50 idle/power, wind and stall warning).

217 compiled menu/save checks, recorder tests and the reviewed menu layout checks passed. The production app compiles with devkitARM. NDSP queue scheduling, device audio output, suspend/resume and coexistence with graphics still require an Azahar/3DS runtime check; pure synthesis tests do not validate those paths. WAV previews are synthesized fixtures, not recordings captured from the device.

Copy the update's `sdmc/` contents onto the existing SD card. Scenery and all three saves remain compatible. CSVs identify model version 23. To test, start a flight, raise/lower throttle, switch aircraft, compare cockpit/exterior mixes, then verify pause, mute and the slow-flight stall cue.

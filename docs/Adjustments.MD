# Sensor Adjustments

This guide covers how to adjust the two user-tunable detection sensors in Sentinel:

- PIR motion on GPIO 27 using the AM312 Mini PIR
- Sound intrusion on GPIO 34 using the MAX9814 microphone board

These settings are runtime-configurable, stored in NVS, and survive reboot.

## Where to Adjust

You can adjust sensor settings in either of these ways:

1. Home Assistant MQTT `number` entities
2. Serial `cfg` commands over USB/serial monitor

If you change a value in Home Assistant, the ESP32 applies it immediately and saves it to NVS.

If you change a value over serial, use:

```text
cfg set <key> <value>
cfg save
cfg apply
```

To see the current values:

```text
cfg show
```

To list all supported config keys:

```text
cfg help
```

## PIR Motion Sensor

The AM312 is a small digital PIR. It does not output an analog level. The firmware smooths it by adding warmup time, detect debounce, clear debounce, and hold time.

### Home Assistant entities

- `Sentinel PIR Warmup`
- `Sentinel PIR Detect Stable`
- `Sentinel PIR Clear Stable`
- `Sentinel PIR Hold`

### Serial config keys

- `pir_warmup_ms`
- `pir_detect_stable_ms`
- `pir_clear_stable_ms`
- `pir_hold_ms`

### Defaults

- `pir_warmup_ms = 30000`
- `pir_detect_stable_ms = 150`
- `pir_clear_stable_ms = 1500`
- `pir_hold_ms = 5000`

### What each setting does

- `pir_warmup_ms`: Ignore PIR changes for this long after boot or PIR re-init. Use this to let the sensor settle.
- `pir_detect_stable_ms`: PIR must stay HIGH this long before motion is declared.
- `pir_clear_stable_ms`: PIR must stay LOW this long before motion is cleared.
- `pir_hold_ms`: After the last valid HIGH, keep motion active for this long.

### How to tune PIR

If the PIR chatters or flickers:

- Increase `pir_detect_stable_ms`
- Increase `pir_clear_stable_ms`
- Increase `pir_hold_ms`

If the PIR feels too slow to react:

- Lower `pir_detect_stable_ms`
- Lower `pir_hold_ms`

If the PIR clears too quickly when a person pauses:

- Increase `pir_clear_stable_ms`
- Increase `pir_hold_ms`

If the PIR takes too long to become usable after boot:

- Lower `pir_warmup_ms` carefully

### Recommended PIR adjustment order

1. Leave `pir_warmup_ms` alone unless boot behavior is a problem.
2. Adjust `pir_detect_stable_ms` first for false triggers.
3. Adjust `pir_clear_stable_ms` next for chattering clear/re-detect cycles.
4. Adjust `pir_hold_ms` last for how long Home Assistant shows motion.

### Example serial commands

```text
cfg set pir_detect_stable_ms 250
cfg set pir_clear_stable_ms 2000
cfg set pir_hold_ms 7000
cfg save
cfg apply
```

## Sound Sensor

The MAX9814 output is an analog audio waveform with DC bias. Sentinel does not use the raw ADC peak anymore. It measures peak-to-peak swing over a sample window and compares that against a threshold.

That means the sound detector is tuned in firmware by detection settings, not by trying to interpret the biased analog level directly.

### Important note about AGC

The MAX9814 breakout handles gain and AGC behavior in hardware. The ESP32 firmware cannot directly tune the MAX9814 AGC algorithm.

What the firmware can tune is:

- how long it samples the microphone
- how large the detected signal swing must be
- how long the sound intrusion stays latched after detection

### Home Assistant entities

- `Sentinel Sound Sample Window`
- `Sentinel Sound Hold`
- `Sentinel Sound Threshold`

### Serial config keys

- `sound_sample_window_ms`
- `sound_hold_ms`
- `sound_threshold_adc`

### Defaults

- `sound_sample_window_ms = 100`
- `sound_hold_ms = 3000`
- `sound_threshold_adc = 1200`

### What each setting does

- `sound_sample_window_ms`: How long the ESP32 samples the microphone before deciding whether a sound event happened.
- `sound_hold_ms`: After a sound intrusion is detected, keep the binary sensor active for this long before clearing.
- `sound_threshold_adc`: Required peak-to-peak ADC swing needed to declare sound intrusion.

### How to tune sound

If ordinary room noise triggers the sensor:

- Increase `sound_threshold_adc`
- Increase `sound_sample_window_ms` slightly if short spikes are the problem

If loud knocks or claps are missed:

- Decrease `sound_threshold_adc`
- Decrease `sound_sample_window_ms` if you want to catch short sharp transients faster

If Home Assistant clears the sound event too fast:

- Increase `sound_hold_ms`

If the sound event stays active too long:

- Decrease `sound_hold_ms`

### Recommended sound adjustment order

1. Adjust `sound_threshold_adc` first.
2. Adjust `sound_sample_window_ms` second.
3. Adjust `sound_hold_ms` last for Home Assistant behavior.

### Example serial commands

```text
cfg set sound_threshold_adc 1500
cfg set sound_sample_window_ms 120
cfg set sound_hold_ms 4000
cfg save
cfg apply
```

## Safe Tuning Workflow

Use this process for either sensor:

1. Change one setting at a time.
2. Test with real behavior in the target room.
3. Keep notes on what improved or got worse.
4. Save only when the result is clearly better.

If you are tuning in Home Assistant, make one adjustment, trigger the sensor a few times, then adjust again. Do not change several values at once or you will not know which change fixed the behavior.

## Reset Strategy

If tuning gets messy, return to the defaults listed above.

You can also clear runtime settings from NVS and fall back to compile-time defaults using the existing serial config reset flow:

```text
cfg reset
cfg load
cfg apply
```

## Quick Reference

Use these when behavior looks wrong:

- False PIR triggers: raise `pir_detect_stable_ms`
- PIR clears too fast: raise `pir_clear_stable_ms` or `pir_hold_ms`
- PIR feels sluggish: lower `pir_detect_stable_ms`
- Sound triggers too easily: raise `sound_threshold_adc`
- Sound misses sharp impacts: lower `sound_threshold_adc` or shorten `sound_sample_window_ms`
- Sound event duration is wrong in HA: adjust `sound_hold_ms`
# Sensor Adjustments

This guide covers the active ESPHome-based Sentinel node defined in `esphome/sentinel.yaml`.

It applies to the current ESPHome build only. The old PlatformIO project in `pio-ARCHIVE/` is deprecated and intentionally excluded from this guide.

## What Is Adjustable

The active ESPHome node exposes these user-tunable features through Home Assistant:

- PIR motion timing
- Sound intrusion timing and thresholding
- Sound-level calibration helpers
- Smoke alarm tone and cadence detection
- Status LED automatic behavior
- Sensor enable and disable switches

The live hardware and runtime surface in the ESPHome build also includes:

- BME280 temperature, humidity, pressure, comfort, and air-quality telemetry
- Reed-switch door state
- Device-health diagnostics
- WS2812 status LEDs

## Where To Adjust Things

Adjustments are made through the ESPHome entities exposed to Home Assistant.

There is no active serial `cfg` workflow, MQTT runtime config path, or BLE beacon watchlist in the current ESPHome implementation.

Most tuning values are exposed as `number`, `switch`, `select`, `button`, `sensor`, `binary_sensor`, or `text_sensor` entities. Values marked with `restore_value` or backed by restored globals survive reboot.

## Sensor Enable Switches

These switches control whether the related logic is running:

- `Enable PIR Sensing`
- `Enable Sound Sensing`
- `Enable Smoke Alarm Sensing`
- `Enable Status LEDs Auto`

Notes:

- Turning off `Enable PIR Sensing` clears the current motion state and stops PIR processing.
- Turning off `Enable Sound Sensing` clears sound and smoke-related runtime states because smoke detection depends on the microphone path.
- `Enable Smoke Alarm Sensing` only enables the smoke detector logic. It still depends on `Enable Sound Sensing` being on.
- `Enable Status LEDs Auto` switches the LED strip between automatic status display and manual effect testing.

## PIR Motion Sensor

The PIR logic in `esphome/sentinel.yaml` uses warmup, stable-detect timing, stable-clear timing, and hold time instead of treating the sensor as a simple immediate on or off edge.

### Home Assistant entities

- `Enable PIR Sensing`
- `PIR Warmup`
- `PIR Detect Stable`
- `PIR Clear Stable`
- `PIR Hold`
- `Motion`
- `PIR Stuck High Fault`

### Defaults from the ESPHome config

- `PIR Warmup = 30000 ms`
- `PIR Detect Stable = 150 ms`
- `PIR Clear Stable = 1500 ms`
- `PIR Hold = 5000 ms`

### What each setting does

- `PIR Warmup`: Ignore PIR activity for this long after boot or after PIR sensing is re-enabled.
- `PIR Detect Stable`: PIR must stay high for at least this long before `Motion` turns on.
- `PIR Clear Stable`: PIR must stay low for at least this long before the system considers clearing motion.
- `PIR Hold`: Once motion is valid, keep `Motion` on for at least this long after the last qualifying high period.

### How to tune PIR

If motion chatters or flickers:

- Raise `PIR Detect Stable`
- Raise `PIR Clear Stable`
- Raise `PIR Hold`

If motion feels too slow:

- Lower `PIR Detect Stable`
- Lower `PIR Hold`

If motion clears too quickly when a person pauses:

- Raise `PIR Clear Stable`
- Raise `PIR Hold`

If the PIR takes too long to become usable after restart:

- Lower `PIR Warmup` carefully

### Recommended PIR adjustment order

1. Leave `PIR Warmup` alone unless startup behavior is the real problem.
2. Adjust `PIR Detect Stable` first to control false triggers.
3. Adjust `PIR Clear Stable` next to stop clear and re-detect chatter.
4. Adjust `PIR Hold` last for the Home Assistant experience you want.

## Door Sensor

The current ESPHome build exposes a `Door` binary sensor from the reed switch.

There are no runtime adjustment entities for the door input in the current ESPHome config. If door behavior needs to change, that requires editing `esphome/sentinel.yaml` rather than tuning from Home Assistant.

## Sound Sensor

The MAX9814 microphone path uses peak-to-peak ADC swing over a sample window. The current ESPHome build does not treat the biased analog signal as a directly meaningful raw loudness value.

The important distinction is:

- `Sound Threshold ADC` controls whether `Sound Intrusion` turns on
- `Sound Level Floor ADC` and `Sound Level Ceiling ADC` only control relative sound display scaling

### Home Assistant entities

Core sound detection:

- `Enable Sound Sensing`
- `Sound Sample Window`
- `Sound Hold`
- `Sound Threshold ADC`
- `Sound Intrusion`

Calibration and display helpers:

- `Sound Level Floor ADC`
- `Sound Level Ceiling ADC`
- `Sound dB Floor`
- `Sound dB Ceiling`
- `Sound Calibration Capture Duration`
- `Capture Quiet Baseline`
- `Capture Loud Reference`
- `Sound Calibration Status`
- `Sound Calibration Captured Peak`
- `Sound Peak ADC`
- `Sound Min ADC`
- `Sound Max ADC`
- `Sound Level %`
- `Sound Approx dB`

### Defaults from the ESPHome config

- `Sound Sample Window = 100 ms`
- `Sound Hold = 3000 ms`
- `Sound Threshold ADC = 1200`
- `Sound Level Floor ADC = 60`
- `Sound Level Ceiling ADC = 1800`
- `Sound dB Floor = 35`
- `Sound dB Ceiling = 90`
- `Sound Calibration Capture Duration = 10000 ms`

### What each setting does

- `Sound Sample Window`: How long the ESP32 samples audio before evaluating the current peak-to-peak swing.
- `Sound Hold`: How long `Sound Intrusion` remains on after a valid event.
- `Sound Threshold ADC`: Minimum peak-to-peak swing needed to declare a sound intrusion.
- `Sound Level Floor ADC`: Peak-to-peak value treated as `0%` sound.
- `Sound Level Ceiling ADC`: Peak-to-peak value treated as `100%` sound.
- `Sound dB Floor`: Approximate dB mapped to `0%` sound.
- `Sound dB Ceiling`: Approximate dB mapped to `100%` sound.
- `Sound Calibration Capture Duration`: How long the guided quiet and loud capture buttons listen before saving a result.

### How to tune sound intrusion

If ordinary room noise triggers `Sound Intrusion`:

- Raise `Sound Threshold ADC`
- Raise `Sound Sample Window` slightly if short spikes are the problem

If sharp impacts are missed:

- Lower `Sound Threshold ADC`
- Lower `Sound Sample Window` if you need to catch faster transients

If the sound event clears too fast:

- Raise `Sound Hold`

If the sound event stays active too long:

- Lower `Sound Hold`

### Recommended sound adjustment order

1. Adjust `Sound Threshold ADC` first.
2. Adjust `Sound Sample Window` second.
3. Adjust `Sound Hold` last for the event duration you want in Home Assistant.

## Calibrating Sound Level Percent And Approx dB

These calibration entities are optional. They improve the usefulness of the relative sound display, but they do not decide whether `Sound Intrusion` triggers.

### Assisted calibration

1. Set `Sound Calibration Capture Duration` to a useful window such as `10000 ms`.
2. Keep the room in its normal quiet state.
3. Press `Capture Quiet Baseline`.
4. Wait for `Sound Calibration Status` to return to `Idle`.
5. The result is written into `Sound Level Floor ADC`.
6. Create a repeatable loud reference near the device.
7. Press `Capture Loud Reference`.
8. Wait for `Sound Calibration Status` to return to `Idle`.
9. The result is written into `Sound Level Ceiling ADC`.
10. Fine-tune `Sound Level Floor ADC`, `Sound Level Ceiling ADC`, and `Sound Threshold ADC` manually if needed.

During capture, `Sound Calibration Captured Peak` shows the highest peak seen so far.

### Direct manual calibration

1. Let the room settle into its normal quiet state.
2. Watch `Sound Peak ADC` for a while and note the typical quiet value.
3. Set `Sound Level Floor ADC` slightly above that baseline.
4. Create a repeatable loud sound you care about.
5. Watch `Sound Peak ADC` and note the typical loud value.
6. Set `Sound Level Ceiling ADC` near that loud value.
7. Choose a reasonable `Sound dB Floor` and `Sound dB Ceiling` pair if you want an approximate dB estimate.

Example starting point:

- Quiet room peak-to-peak: `60`
- Loud reference peak-to-peak: `1800`
- `Sound Level Floor ADC = 60`
- `Sound Level Ceiling ADC = 1800`
- `Sound dB Floor = 35`
- `Sound dB Ceiling = 90`

Important limits:

- The MAX9814 uses AGC, so absolute loudness is not stable like a calibrated sound meter.
- The ESP32 ADC is not a precision audio front end.
- `Sound Approx dB` is best treated as an automation-oriented estimate, not an instrument-grade reading.

## Smoke Alarm Sensing

The ESPHome build can run a best-effort smoke alarm detector from the microphone path. It looks for a combination of:

- sufficient sound strength
- energy near common alarm-tone frequencies
- repeated beep timing that matches an alarm pattern better than random room noise

This is useful for automations and notifications. It is not a certified life-safety detector.

### Home Assistant entities

- `Enable Smoke Alarm Sensing`
- `Smoke Alarm Profile`
- `Smoke Alarm Tone Threshold`
- `Smoke Alarm Min Peak ADC`
- `Smoke Alarm Hold`
- `Smoke Alarm Confirm Beeps`
- `Smoke Alarm Custom Tone 1`
- `Smoke Alarm Custom Tone 2`
- `Smoke Alarm Custom Tone 3`
- `Smoke Alarm Custom Min Beep Gap`
- `Smoke Alarm Custom Max Beep Gap`
- `Smoke Alarm Tone Score`
- `Smoke Alarm Beep Count`
- `Smoke Alarm Last Beep Age`
- `Smoke Alarm Detected`
- `Smoke Monitor Fault`

### Defaults from the ESPHome config

- `Enable Smoke Alarm Sensing = off`
- `Smoke Alarm Profile = Default`
- `Smoke Alarm Tone Threshold = 18`
- `Smoke Alarm Min Peak ADC = 250`
- `Smoke Alarm Hold = 15000 ms`
- `Smoke Alarm Confirm Beeps = 3`
- `Smoke Alarm Custom Tone 1 = 3000 Hz`
- `Smoke Alarm Custom Tone 2 = 3200 Hz`
- `Smoke Alarm Custom Tone 3 = 3400 Hz`
- `Smoke Alarm Custom Min Beep Gap = 250 ms`
- `Smoke Alarm Custom Max Beep Gap = 2000 ms`

### Smoke alarm profiles

The current select options are:

- `Default`
- `Generic Smoke`
- `Strict T3 Smoke`
- `Strict T4 CO`
- `Low Battery Chirp`
- `Custom`

Use the closest preset first before manually tuning thresholds.

### Recommended smoke alarm adjustment order

1. Turn on `Enable Smoke Alarm Sensing`.
2. Choose the closest `Smoke Alarm Profile`.
3. Test with a real alarm self-test tone if possible.
4. Watch `Smoke Alarm Tone Score`, `Smoke Alarm Beep Count`, and `Smoke Alarm Last Beep Age` while testing.
5. If you are using `Default` or `Custom`, adjust `Smoke Alarm Tone Threshold` so normal sounds stay below it and real alarm tones rise above it.
6. If you are using `Default` or `Custom`, adjust `Smoke Alarm Min Peak ADC` if distant speech or clatter still gets counted.
7. If you are using `Default` or `Custom`, adjust `Smoke Alarm Confirm Beeps` if you want detection faster or slower.
8. If you are using `Custom`, tune the three tone targets and the custom min and max gap values.
9. Adjust `Smoke Alarm Hold` last for Home Assistant event duration.

### How to read the smoke debug entities

- `Smoke Alarm Tone Score` should rise during a real alarm tone and stay lower during unrelated room sound.
- `Smoke Alarm Beep Count` should climb toward the configured confirmation count during a valid alarm sequence.
- `Smoke Alarm Last Beep Age` should stay relatively low while the alarm cadence continues, then grow once the sequence stops.
- `Smoke Monitor Fault` means smoke monitoring was enabled while sound sensing was not available.

If tone score rises but beep count does not, the cadence is not matching closely enough or the minimum peak threshold is still too strict.

## Device Health

The ESPHome build exposes health and fault entities so you can distinguish sensor tuning problems from connectivity or hardware faults.

### Home Assistant entities

- `Device Health`
- `Reset Reason`
- `Boot Count`
- `WiFi Connected`
- `API Connected`
- `WiFi Disconnect Count`
- `API Disconnect Count`
- `Heap Free`
- `Heap Min Free`
- `Heap Fragmentation`
- `Main Loop Time`
- `Environment Sensor Fault`
- `PIR Stuck High Fault`
- `Smoke Monitor Fault`
- `Memory Warning`

### What they mean

- `Device Health`: Summary state used by dashboards and automation logic. It can report states such as `Recovering After Boot`, `WiFi Reconnecting`, `WiFi OK / API Idle`, `Sensor Trouble`, `Low Memory`, `Memory Fragmented`, or `Healthy`.
- `Environment Sensor Fault`: The BME280 path is unavailable or returning invalid readings.
- `PIR Stuck High Fault`: The PIR input appears stuck high long enough to be suspicious.
- `Smoke Monitor Fault`: Smoke monitoring was enabled while the sound path was not active.
- `Memory Warning`: Heap free space or fragmentation has crossed the warning threshold.

### How to use them

- If `Device Health` stays on `Recovering After Boot`, the node is still settling after startup.
- If it shows `WiFi Reconnecting`, fix power or network quality before changing sensor thresholds.
- If it shows `WiFi OK / API Idle`, Wi-Fi is up but Home Assistant is not actively connected through the ESPHome API.
- If it shows `Sensor Trouble`, investigate the BME280, PIR state, or smoke-monitor dependency path before tuning detection values.
- If it shows `Low Memory` or `Memory Fragmented`, treat that as a stability warning.

## Status LEDs

The current ESPHome build drives a three-pixel WS2812 status strip on `GPIO25` by default.

### Home Assistant entities

- `Status LEDs`
- `Enable Status LEDs Auto`
- `Status LED Left Meaning`
- `Status LED Center Meaning`
- `Status LED Right Meaning`

### LED roles in auto mode

- Left LED: system, network, and overall health
- Center LED: live sensing activity
- Right LED: urgent alerts and sensor faults

### Manual test effects

When `Enable Status LEDs Auto` is off, the `Status LEDs` light can use these named effects:

- `Test System Healthy`
- `Test Network Reconnect`
- `Test API Idle`
- `Test Motion`
- `Test Sound`
- `Test Quiet Calibration`
- `Test Loud Calibration`
- `Test Smoke Alert`
- `Test Sensor Fault`
- `Test Memory Warning`
- `Test Rainbow`
- `Full Status Preview`

### Default auto color map

Left LED:

- Dim green: healthy idle
- Amber pulse: booting or PIR warmup
- Teal blink: Wi-Fi is up but no API client is connected
- Amber blink: Wi-Fi reconnecting
- Purple pulse: memory warning
- Red or amber failure indication: node status problem

Center LED:

- Off: idle
- Blue pulse: motion activity
- Orange pulse: sound activity
- Cyan pulse: quiet calibration capture
- Yellow pulse: loud calibration capture

Right LED:

- Off: no active alert
- Fast red flash: smoke alarm detected
- Yellow blink: sensor trouble

## Safe Tuning Workflow

Use this process for any sensor adjustment:

1. Change one value at a time.
2. Test with real behavior in the actual room.
3. Watch the related binary sensor, helper sensors, and fault indicators together.
4. Keep notes on what improved or got worse.
5. Only keep the change if the result is clearly better.

## Quick Reference

Use these when behavior looks wrong:

- False PIR triggers: raise `PIR Detect Stable`
- PIR clears too fast: raise `PIR Clear Stable` or `PIR Hold`
- PIR feels sluggish: lower `PIR Detect Stable`
- Sound triggers too easily: raise `Sound Threshold ADC`
- Sound misses sharp impacts: lower `Sound Threshold ADC` or shorten `Sound Sample Window`
- Sound event duration is wrong: adjust `Sound Hold`
- Smoke alarm detection false-triggers: raise `Smoke Alarm Tone Threshold`, `Smoke Alarm Min Peak ADC`, or `Smoke Alarm Confirm Beeps`
- Smoke alarm detection misses real tests: lower `Smoke Alarm Tone Threshold`, `Smoke Alarm Min Peak ADC`, or `Smoke Alarm Confirm Beeps`

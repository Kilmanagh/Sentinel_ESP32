# Sensor Adjustments

This guide covers how to adjust the user-tunable detection features in Sentinel:

- PIR motion on GPIO 27 using the AM312 Mini PIR
- Sound intrusion on GPIO 34 using the MAX9814 microphone board
- BLE beacon watchlist presence tracking

These settings are runtime-configurable, stored in NVS, and survive reboot.

## Where to Adjust

You can adjust settings in either of these ways:

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

## BLE Beacon Watchlist

The old BLE snooper is gone. Sentinel now uses a fixed watchlist of up to 4 known beacons.

This is intentionally lightweight:

- passive BLE scan
- no track-everything device table
- no generic phone snooping
- only configured iBeacon UUID/major/minor identifiers are matched

This works best with dedicated iBeacon-compatible tags or apps that advertise a stable iBeacon identity. It is not recommended for phones unless you control the beacon app and keep its UUID/major/minor stable.

### How the watchlist is stored

Beacon slots are stored in ESP32 NVS inside the same runtime config namespace as the other settings.

Each slot has:

- a user label
- an iBeacon UUID
- an iBeacon major value
- an iBeacon minor value

The firmware currently supports 4 slots.

### Home Assistant entities

Presence entities:

- `Sentinel BLE Beacons Present`
- `Sentinel Beacon 1`
- `Sentinel Beacon 2`
- `Sentinel Beacon 3`
- `Sentinel Beacon 4`

Home Assistant config entities:

- `Sentinel Beacon 1 Name`
- `Sentinel Beacon 1 UUID`
- `Sentinel Beacon 1 Major`
- `Sentinel Beacon 1 Minor`
- `Sentinel Beacon 2 Name`
- `Sentinel Beacon 2 UUID`
- `Sentinel Beacon 2 Major`
- `Sentinel Beacon 2 Minor`
- `Sentinel Beacon 3 Name`
- `Sentinel Beacon 3 UUID`
- `Sentinel Beacon 3 Major`
- `Sentinel Beacon 3 Minor`
- `Sentinel Beacon 4 Name`
- `Sentinel Beacon 4 UUID`
- `Sentinel Beacon 4 Major`
- `Sentinel Beacon 4 Minor`

### Serial config keys

- `ble_beacon1_name`
- `ble_beacon1_uuid`
- `ble_beacon1_major`
- `ble_beacon1_minor`
- `ble_beacon2_name`
- `ble_beacon2_uuid`
- `ble_beacon2_major`
- `ble_beacon2_minor`
- `ble_beacon3_name`
- `ble_beacon3_uuid`
- `ble_beacon3_major`
- `ble_beacon3_minor`
- `ble_beacon4_name`
- `ble_beacon4_uuid`
- `ble_beacon4_major`
- `ble_beacon4_minor`

### Defaults

- Slot names default to `Beacon 1` through `Beacon 4`
- Slot UUIDs default to empty
- Slot major/minor values default to `0`
- Presence timeout is about 150 seconds

### Serial enrollment workflow

If you do not already know the beacon identity, use the serial enrollment logger first.

Commands:

```text
ble help
ble enroll on
ble enroll status
ble enroll off
```

While `ble enroll on` is active, the ESP32 prints nearby iBeacon advertisements in this format:

```text
[BLE][ENROLL] UUID:12345678-1234-1234-1234-1234567890AB Major:100 Minor:7 RSSI:-61
```

Duplicate sightings are suppressed for about 30 seconds per beacon identity so the serial console stays readable.

### Home Assistant workflow

1. Open the Sentinel device in Home Assistant.
2. Find the `Sentinel Beacon X Name` text entity for the slot you want.
3. Enter a friendly label such as `Keys`, `Backpack`, or `Dad Tag`.
4. Enter the beacon UUID in `Sentinel Beacon X UUID`.
5. Enter the beacon major in `Sentinel Beacon X Major`.
6. Enter the beacon minor in `Sentinel Beacon X Minor`.
7. The ESP32 validates, saves to NVS, republishes state, and starts watching that beacon identity.

If you change the UUID, major, or minor, the watchlist updates immediately.

If you change the name, Home Assistant may take a moment to reflect the updated label because HA can cache entity names, but the stored value on the device is still correct.

### Accepted UUID formats

These are accepted:

- `12345678-1234-1234-1234-1234567890AB`
- `123456781234123412341234567890AB`
- `12345678 1234 1234 1234 1234567890AB`

The firmware normalizes the UUID before storing it.

Major and minor must each be in the range `0` to `65535`.

### Clearing a slot

To empty a slot in serial, set the UUID to `CLEAR`:

```text
cfg set ble_beacon1_uuid CLEAR
cfg save
cfg apply
```

This clears the UUID and resets the slot's major and minor to `0`.

In Home Assistant, clearing the UUID text value should remove the slot from the watchlist if HA sends an empty string. If that is inconsistent in the UI, use serial with `CLEAR` for the most reliable reset.

### Example serial setup

```text
cfg set ble_beacon1_name Keys
cfg set ble_beacon1_uuid 12345678-1234-1234-1234-1234567890AB
cfg set ble_beacon1_major 100
cfg set ble_beacon1_minor 7
cfg save
cfg apply
```

### How presence works

- If a configured beacon is seen during scans, its slot becomes `home`.
- If it is not seen again for about 150 seconds, its slot becomes `away`.
- Unconfigured slots are ignored.
- A beacon only matches when UUID, major, and minor all match the configured slot.

### Watchlist tuning notes

- Use stable dedicated beacons instead of phones.
- Keep the watchlist small and intentional.
- If a beacon feels too slow to clear, that is the current timeout behavior, not a sensor bug.
- If a beacon rarely appears, verify the UUID, major, and minor first before assuming radio problems.

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

If you are using the ESPHome config at `esphome/sentinel.yaml`, you can also expose:

- `Sound Level Floor ADC`
- `Sound Level Ceiling ADC`
- `Sound dB Floor`
- `Sound dB Ceiling`
- `Sound Level %`
- `Sound Approx dB`
- `Sound Calibration Capture Duration`
- `Capture Quiet Baseline`
- `Capture Loud Reference`
- `Sound Calibration Status`
- `Sound Calibration Captured Peak`
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

For the ESPHome calibration entities in `esphome/sentinel.yaml`:

- `Sound Level Floor ADC`: The peak-to-peak value that should be treated as `0%` sound.
- `Sound Level Ceiling ADC`: The peak-to-peak value that should be treated as `100%` sound.
- `Sound dB Floor`: The approximate dB value you want mapped to `0%`.
- `Sound dB Ceiling`: The approximate dB value you want mapped to `100%`.
- `Sound Level %`: Relative loudness scaled between the floor and ceiling ADC values.
- `Sound Approx dB`: A linear estimate derived from `Sound Level %` and the dB floor/ceiling values.
- `Sound Calibration Capture Duration`: How long the assisted calibration buttons listen before saving a captured value.
- `Capture Quiet Baseline`: Starts a timed quiet-room capture and writes the result into `Sound Level Floor ADC`.
- `Capture Loud Reference`: Starts a timed loud-reference capture and writes the result into `Sound Level Ceiling ADC`.
- `Sound Calibration Status`: Shows whether the device is idle or currently capturing quiet or loud sound.
- `Sound Calibration Captured Peak`: Shows the highest peak captured during the current assisted calibration run.

Important distinction:

- `Sound Threshold ADC` controls whether `Sound Intrusion` turns on.
- `Sound Level Floor ADC` and `Sound Level Ceiling ADC` only control how the relative `Sound Level %` display is scaled from quiet to loud.

The percentage display does not trigger detection by itself.

You can now calibrate in two ways:

- assisted manual: use the capture buttons to fill in a first floor and ceiling automatically from live sound
- direct manual: edit `Sound Level Floor ADC`, `Sound Level Ceiling ADC`, and `Sound Threshold ADC` yourself for final tuning

For the ESPHome smoke alarm detector:

- `Enable Smoke Alarm Sensing`: Turns the tone-and-cadence detector on or off.
- `Smoke Alarm Profile`: Selects which preset tone-and-cadence profile is active.
- `Smoke Alarm Tone Threshold`: Minimum tone score required before a beep is considered a smoke alarm tone.
- `Smoke Alarm Min Peak ADC`: Minimum overall sound strength required before tone scoring is allowed to count.
- `Smoke Alarm Hold`: How long `Smoke Alarm Detected` stays on after a valid alarm pattern is seen.
- `Smoke Alarm Confirm Beeps`: How many valid beeps must be seen close together before the alarm is declared.
- `Smoke Alarm Custom Tone 1/2/3`: The three target frequencies used only when `Smoke Alarm Profile` is set to `Custom`.
- `Smoke Alarm Custom Min Beep Gap`: The minimum spacing between qualifying beeps when `Custom` is active.
- `Smoke Alarm Custom Max Beep Gap`: The maximum spacing between qualifying beeps when `Custom` is active.
- `Smoke Alarm Tone Score`: Best-effort score showing how much the captured audio resembles the target alarm frequencies.
- `Smoke Alarm Beep Count`: How many qualifying beeps are currently in the active detection sequence.
- `Smoke Alarm Last Beep Age`: How long it has been since the last qualifying beep was seen.
- `Smoke Alarm Detected`: Binary result of the smoke alarm detector.

`Sound Level %` and `Sound Approx dB` are calibration outputs. They are not true SPL meter readings.

`Smoke Alarm Detected` is a convenience automation signal. It is not a certified life-safety detector.

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

### Calibrating Sound Level % and Approx dB

Use this only if you want a relative sound level display in `esphome/sentinel.yaml`.

#### Assisted manual calibration

This is the easiest way to get a first working floor and ceiling.

1. Set `Sound Calibration Capture Duration` to a useful window such as `10000 ms`.
2. Keep the room in its normal quiet state.
3. Press `Capture Quiet Baseline`.
4. Watch `Sound Calibration Status` until it returns to `Idle`.
5. The captured value is written into `Sound Level Floor ADC`.
6. Make a repeatable loud sound near the device.
7. Press `Capture Loud Reference`.
8. Wait for `Sound Calibration Status` to return to `Idle`.
9. The captured value is written into `Sound Level Ceiling ADC`.
10. Fine tune the floor, ceiling, and `Sound Threshold ADC` manually if needed.

During capture, `Sound Calibration Captured Peak` shows the highest peak seen so far.

#### Direct manual calibration

1. Let the room sit in its normal quiet state.
2. Watch `Sound Peak ADC` for 30-60 seconds and note the typical quiet value.
3. Set `Sound Level Floor ADC` just above that quiet baseline.
4. Make a repeatable loud sound you care about, such as clapping, speech near the device, or a test tone.
5. Watch `Sound Peak ADC` and note the typical loud value.
6. Set `Sound Level Ceiling ADC` near that loud value.
7. Pick a reasonable `Sound dB Floor` and `Sound dB Ceiling` pair if you want an approximate dB estimate.

Example starting point:

- Quiet room peak-to-peak: `60`
- Loud reference peak-to-peak: `1800`
- `Sound Level Floor ADC = 60`
- `Sound Level Ceiling ADC = 1800`
- `Sound dB Floor = 35`
- `Sound dB Ceiling = 90`

This gives you:

- `0%` near your quiet room baseline
- `100%` near your chosen loud reference
- an approximate dB figure mapped linearly between those points

You still choose `Sound Threshold ADC` separately based on when you want `Sound Intrusion` to trigger.

The assisted buttons are a starting point, not a lock-in. You can still manually adjust any captured value afterward.

Important limits:

- The MAX9814 uses AGC, so absolute loudness is not stable like a calibrated sound meter.
- The ESP32 ADC is not a precision audio front end.
- `Sound Approx dB` is best treated as a room-specific estimate for automation, not an instrument-grade reading.

### Smoke Alarm Sensing

The ESPHome build can also do a best-effort smoke alarm check using the same microphone.

It looks for:

- strong enough sound to clear a minimum ADC peak threshold
- energy around common alarm frequencies near `3000-3400 Hz`
- repeated beeps close enough together to match an alarm pattern instead of a single random chirp

The detector now supports preset profiles:

- `Default`: Keeps the original current behavior. It uses the existing `Smoke Alarm Tone Threshold`, `Smoke Alarm Min Peak ADC`, `Smoke Alarm Confirm Beeps`, and `Smoke Alarm Hold` values, with the original tone targets near `3000`, `3200`, and `3400 Hz` and beep spacing of about `250-2000 ms`.
- `Generic Smoke`: A broader smoke-alarm preset aimed at common high-pitched residential alarms.
- `Strict T3 Smoke`: A tighter smoke preset biased toward Temporal-3 style patterns.
- `Strict T4 CO`: A preset biased toward 4-beep carbon-monoxide style patterns.
- `Low Battery Chirp`: A single-chirp preset intended for periodic battery warnings.
- `Custom`: Uses your manual `Smoke Alarm Tone Threshold`, `Smoke Alarm Min Peak ADC`, `Smoke Alarm Confirm Beeps`, `Smoke Alarm Hold`, and the custom tone/gap settings.

This works better than plain loudness thresholding, but it is still limited by:

- MAX9814 AGC behavior
- ESP32 ADC quality
- room acoustics and distance from the alarm
- differences between smoke alarm brands and tones

Use it for automation and notifications, not as a substitute for a listed smoke alarm.

### Recommended smoke alarm adjustment order

1. Turn on `Enable Smoke Alarm Sensing`.
2. Choose the closest `Smoke Alarm Profile` first.
3. Test with a real smoke alarm test tone if possible.
4. Watch `Smoke Alarm Tone Score`, `Smoke Alarm Beep Count`, and `Smoke Alarm Last Beep Age` during the beep.
5. If you are on `Default` or `Custom`, adjust `Smoke Alarm Tone Threshold` so normal room sounds stay below it and the alarm tone rises above it.
6. If you are on `Default` or `Custom`, adjust `Smoke Alarm Min Peak ADC` if distant speech or short clatter still triggers the tone detector.
7. If you are on `Default` or `Custom`, adjust `Smoke Alarm Confirm Beeps` if you want detection faster or slower.
8. If you are on `Custom`, tune `Smoke Alarm Custom Tone 1/2/3` and the custom beep-gap values to match your alarm.
9. Adjust `Smoke Alarm Hold` last for Home Assistant behavior.

How to read the debug sensors:

- `Smoke Alarm Tone Score` should rise during the alarm beep and stay lower during normal room sound.
- `Smoke Alarm Beep Count` should climb toward your `Smoke Alarm Confirm Beeps` setting during a real alarm sequence.
- `Smoke Alarm Last Beep Age` should stay relatively low between real alarm beeps and then grow once the pattern stops.

If `Smoke Alarm Tone Score` rises but `Smoke Alarm Beep Count` does not increase, the cadence is not matching closely enough or the minimum peak threshold is still too strict.

### Smoke alarm starting points

- `Default`: your current settings, with factory defaults of `Tone Threshold = 18`, `Min Peak ADC = 250`, `Confirm Beeps = 3`, `Hold = 15000`, and tone targets near `3000`, `3200`, and `3400 Hz`
- `Generic Smoke`: `Tone Threshold = 16`, `Min Peak ADC = 220`, `Confirm Beeps = 3`, `Hold = 15000`, tone targets near `2800`, `3100`, and `3400 Hz`
- `Strict T3 Smoke`: `Tone Threshold = 18`, `Min Peak ADC = 250`, `Confirm Beeps = 3`, `Hold = 15000`, beep spacing about `700-2200 ms`
- `Strict T4 CO`: `Tone Threshold = 20`, `Min Peak ADC = 260`, `Confirm Beeps = 4`, `Hold = 15000`, tone targets near `2600`, `2900`, and `3200 Hz`, beep spacing about `350-1500 ms`
- `Low Battery Chirp`: `Tone Threshold = 22`, `Min Peak ADC = 180`, `Confirm Beeps = 1`, `Hold = 6000`, beep spacing about `10000-90000 ms`
- `Custom`: start from the `Default` values, then tune the custom tone and gap settings to match your device

If normal sounds falsely trigger smoke alarm detection:

- Increase `Smoke Alarm Tone Threshold`
- Increase `Smoke Alarm Min Peak ADC`
- Increase `Smoke Alarm Confirm Beeps`

If a real alarm test is missed:

- Decrease `Smoke Alarm Tone Threshold`
- Decrease `Smoke Alarm Min Peak ADC`
- Decrease `Smoke Alarm Confirm Beeps`

If the smoke alarm event clears too quickly in Home Assistant:

- Increase `Smoke Alarm Hold`

If the smoke alarm event stays active too long after the alarm stops:

- Decrease `Smoke Alarm Hold`

## Device Health

The ESPHome build now also exposes node-health diagnostics so you can tell whether the device is healthy, reconnecting, or starting to struggle before the sensors look broken.

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

### What they mean

- `Device Health`: A single summary state intended for dashboards and troubleshooting. It can report states such as `Recovering After Boot`, `WiFi Reconnecting`, `WiFi OK / API Idle`, `Sensor Trouble`, `Low Memory`, `Memory Fragmented`, or `Healthy`.
- `Reset Reason`: Human-readable reason for the last reboot.
- `Boot Count`: Number of times the node has booted.
- `WiFi Connected`: Whether the node is currently associated with Wi-Fi.
- `API Connected`: Whether at least one ESPHome API client is currently connected.
- `WiFi Disconnect Count`: How many Wi-Fi disconnect events have been seen.
- `API Disconnect Count`: How many API client disconnect events have been seen.
- `Heap Free`: Current free heap in bytes.
- `Heap Min Free`: Lowest free heap seen since boot.
- `Heap Fragmentation`: Heap fragmentation percentage.
- `Main Loop Time`: Longest recent main-loop time reported by ESPHome.

### How to use them

- If `Device Health` sits on `Recovering After Boot`, the node is still settling after startup.
- If it shows `WiFi Reconnecting`, focus on signal quality, power stability, or router behavior before changing sensor thresholds.
- If it shows `WiFi OK / API Idle`, the node is on the network but Home Assistant is not actively connected.
- If it shows `Sensor Trouble`, check whether the BME280 values are unavailable or wiring is intermittent.
- If it shows `Low Memory` or `Memory Fragmented`, treat that as a firmware stability warning and avoid adding more heavy features until the memory picture is understood.

## Status LEDs

The ESPHome build can now drive three chained WS2812 status LEDs so the node can show system health, live activity, and urgent alerts at the same time.

### Default wiring

- Default data pin: `GPIO25`
- Default LED type: `3 x WS2812`
- Default RGB order: `GRB`
- Wiring chain: `GPIO25 -> DIN LED 1 -> DOUT LED 1 -> DIN LED 2 -> DOUT LED 2 -> DIN LED 3`
- Power: `5V` and `GND` to the LED chain, with ESP32 ground shared

Recommended hardware:

- Add a `330-470 ohm` resistor in series with the data line near LED 1
- Add a `470-1000 uF` capacitor across LED `5V` and `GND` near the LED chain

If your breakout uses a different color order, change the YAML substitution for `status_pixel_rgb_order`.

### Home Assistant entities

- `Status LEDs`
- `Enable Status LEDs Auto`
- `Status LED Left Meaning`
- `Status LED Center Meaning`
- `Status LED Right Meaning`

### LED roles

- `LED 1` left: system, network, and overall health
- `LED 2` center: live sensing activity
- `LED 3` right: urgent alerts and sensor faults

### Manual test effects

When `Enable Status LEDs Auto` is off, you can use the `Status LEDs` light entity directly in Home Assistant and select one of these named effects from the light's effect dropdown:

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

### How it works

- `Enable Status LEDs Auto` on: the firmware runs the `Auto Status Layout` effect and assigns each LED a fixed role.
- `Enable Status LEDs Auto` off: the automatic layout stops and the `Status LEDs` entity is available for manual color/effect testing.
- The three `Status LED ... Meaning` text sensors show what each LED is currently representing in Home Assistant, even though the LED chain is still exposed as one `Status LEDs` light entity.

### Default color map

`LED 1` system / network / health:

- Dim green: healthy idle
- Amber pulse: booting or PIR warmup
- Teal blink: Wi-Fi is up but no API client is connected
- Amber blink: Wi-Fi reconnecting
- Purple pulse: memory pressure or fragmentation warning
- Red blink: node status failure

`LED 2` live activity:

- Off: idle
- Blue pulse: motion active
- Orange pulse: sound intrusion active
- Cyan pulse: quiet calibration capture running
- Yellow pulse: loud calibration capture running

`LED 3` alerts:

- Off: no active alert
- Fast red flash: smoke alarm detected
- Yellow blink: sensor trouble

### Color scheme reference

- `Green`: healthy / stable / normal idle
- `Amber`: booting, warmup, or reconnecting
- `Teal/Cyan`: connected to Wi-Fi but waiting on API or doing quiet calibration
- `Blue`: motion-related state
- `Orange`: sound-related alert state
- `Yellow`: warning or fault that is not smoke
- `Purple`: resource-health warning such as memory pressure
- `Red`: highest-priority alert, used for smoke detection

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

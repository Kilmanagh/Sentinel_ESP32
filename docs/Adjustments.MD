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
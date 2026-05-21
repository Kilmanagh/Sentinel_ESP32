# Sentinel Lovelace Dashboard

This guide ships a clean Home Assistant Lovelace dashboard for the ESPHome Sentinel node.

Files:

- `docs/Sentinel_Lovelace_Dashboard.yaml` - built-in Lovelace dashboard YAML
- `docs/Adjustments.MD` - tuning and calibration reference

## Goal

The dashboard is split into three views so the device is easier to use day-to-day:

- `Overview` - live status, environment, sound, and smoke alarm readouts
- `Settings` - tuning values, sensing toggles, LED controls, calibration actions, and restart/reset controls
- `Troubleshooting` - high-signal debug readouts, LED checks, and recovery actions

## Why this layout works

Home Assistant device pages often mix editable numbers, action buttons, and live sensors together. That makes a heavily tunable device feel cluttered.

This dashboard separates them by intent:

- display what the room and device are doing right now
- adjust detection and calibration settings in one place
- troubleshoot false positives and missed detections without hunting through the device page

## Install

### Option 1: YAML dashboard

1. In Home Assistant, open `Settings > Dashboards`.
2. Create a new dashboard or open an existing YAML-mode dashboard.
3. Copy the contents of `docs/Sentinel_Lovelace_Dashboard.yaml` into the dashboard editor.
4. Save and reload the dashboard.

### Option 2: Manual card copy

If you do not want a full YAML dashboard, copy individual cards or sections from `docs/Sentinel_Lovelace_Dashboard.yaml` into a Lovelace view.

## Entity ID assumptions

The dashboard assumes your ESPHome device is named `Sentinel` and that Home Assistant generated entity IDs such as:

- `sensor.sentinel_temperature`
- `binary_sensor.sentinel_motion`
- `switch.sentinel_enable_sound_sensing`
- `button.sentinel_capture_quiet_baseline`

If your names differ, update the `entity:` lines in `docs/Sentinel_Lovelace_Dashboard.yaml`.

## Recommended use

### Overview view

Use this view for normal daily operation.

Focus on:

- `Motion`, `Door`, `Sound Intrusion`, `Smoke Alarm`
- `Temperature`, `Humidity`, `Pressure`, `Comfort Index`, `IAQ Score`
- `Sound Level %`, `Sound Approx dB`, `Smoke Alarm Tone Score`
- `Device Health`, `Status LED Left Meaning`, `Status LED Center Meaning`, `Status LED Right Meaning`, `Status LEDs`, `Enable Status LEDs Auto`

### Settings view

Use this view when you are tuning behavior.

PIR settings:

- `PIR Warmup`
- `PIR Detect Stable`
- `PIR Clear Stable`
- `PIR Hold`

Sound settings:

- `Sound Sample Window`
- `Sound Hold`
- `Sound Threshold ADC`
- `Sound Level Floor ADC`
- `Sound Level Ceiling ADC`
- `Sound dB Floor`
- `Sound dB Ceiling`

Assisted calibration actions:

- `Capture Quiet Baseline`
- `Capture Loud Reference`
- `Sound Calibration Capture Duration`

Smoke alarm tuning:

- `Enable Smoke Alarm Sensing`
- `Smoke Alarm Tone Threshold`
- `Smoke Alarm Min Peak ADC`
- `Smoke Alarm Confirm Beeps`
- `Smoke Alarm Hold`

Status LED controls:

- `Enable Status LEDs Auto`
- `Status LEDs`
- `Status LED Left Meaning`
- `Status LED Center Meaning`
- `Status LED Right Meaning`
- the current `effect` attribute when you are testing patterns manually

### Troubleshooting view

Use this when a sensor seems wrong.

Helpful readouts:

- `Device Health`
- `Status LED Left Meaning`
- `Status LED Center Meaning`
- `Status LED Right Meaning`
- `Sound Peak ADC`
- `Sound Level %`
- `Sound Approx dB`
- `Smoke Alarm Tone Score`
- `Smoke Alarm Beep Count`
- `Smoke Alarm Last Beep Age`
- `Sound Calibration Status`
- `Sound Calibration Captured Peak`

Helpful LED checks:

- `Status LEDs`
- `Enable Status LEDs Auto`
- the active LED `effect` attribute

Helpful actions:

- `Capture Quiet Baseline`
- `Capture Loud Reference`
- `Reset Detection State`
- `Restart Device`

## Calibration workflow

### Sound level calibration

1. Open the `Settings` view.
2. Set `Sound Calibration Capture Duration` to something practical like `10000 ms`.
3. With the room quiet, press `Capture Quiet Baseline`.
4. Wait for `Sound Calibration Status` to return to `Idle`.
5. Create a repeatable loud sound and press `Capture Loud Reference`.
6. Wait for `Sound Calibration Status` to return to `Idle`.
7. Fine tune `Sound Level Floor ADC`, `Sound Level Ceiling ADC`, and `Sound Threshold ADC` manually.

Important distinction:

- `Sound Threshold ADC` controls detection
- `Sound Level Floor ADC` and `Sound Level Ceiling ADC` control percentage scaling

### Smoke alarm tuning

1. Turn on `Enable Smoke Alarm Sensing`.
2. Run a real alarm test tone if possible.
3. Watch `Smoke Alarm Tone Score`, `Smoke Alarm Beep Count`, and `Smoke Alarm Last Beep Age`.
4. Adjust `Smoke Alarm Tone Threshold`, `Smoke Alarm Min Peak ADC`, and `Smoke Alarm Confirm Beeps`.
5. Adjust `Smoke Alarm Hold` last.

## Troubleshooting

### Sound intrusion triggers too easily

- Raise `Sound Threshold ADC`
- Raise `Sound Sample Window` slightly if very short spikes are the issue
- Re-capture the quiet baseline if the room changed

### Sound intrusion misses obvious noise

- Lower `Sound Threshold ADC`
- Lower `Sound Sample Window` if you want sharper transient response
- Re-capture the loud reference if the current ceiling is too high or too low

### Sound level percentage looks wrong

- Re-capture the quiet baseline
- Re-capture the loud reference
- Manually fine tune `Sound Level Floor ADC` and `Sound Level Ceiling ADC`

### Smoke alarm false triggers

- Raise `Smoke Alarm Tone Threshold`
- Raise `Smoke Alarm Min Peak ADC`
- Raise `Smoke Alarm Confirm Beeps`

### Smoke alarm misses the test tone

- Lower `Smoke Alarm Tone Threshold`
- Lower `Smoke Alarm Min Peak ADC`
- Lower `Smoke Alarm Confirm Beeps`
- Confirm that `Smoke Alarm Tone Score` rises when the alarm sounds

### A detector seems stuck

Use `Reset Detection State` first. That clears live detection state without wiping your saved settings.

### The status LEDs are dark or the colors look wrong

- Confirm the LEDs are powered from `5V` and share `GND` with the ESP32
- Confirm the data chain order is `LED 1 -> LED 2 -> LED 3`
- If colors are swapped, adjust `status_pixel_rgb_order` in the ESPHome YAML
- Turn off `Enable Status LEDs Auto` and run `Full Status Preview` from the `Status LEDs` entity

## Notes

- This dashboard uses built-in Lovelace cards only.
- It is meant to complement the device page, not replace your tuning guide.
- If Home Assistant cached older entity categories or names, reload the ESPHome device or re-open the dashboard after updates.

# Sentinel Home Assistant Helpers

This guide covers the pieces that are better kept in Home Assistant instead of pushed into the ESP32 firmware:

- event counters
- last-event tracking
- a simple mode selector

The example package file is:

- `home_assistant/sentinel_helpers_package.yaml`

## Why this split exists

The ESP32 firmware already carries the always-on sensing logic, signal processing, LED logic, and diagnostics. Counters, helper selectors, and timestamp bookkeeping are cheaper and easier to change in Home Assistant.

This keeps the node lighter while still giving you:

- per-event counters
- a last-event summary
- a mode helper that can turn the existing ESPHome switches on and off

## What stays on the ESP32

These remain firmware-side because they reflect device-local state or actions:

- fault flags such as `Environment Sensor Fault`, `PIR Stuck High Fault`, `Smoke Monitor Fault`, and `Memory Warning`
- maintenance actions such as `Capture Quiet Baseline`, `Capture Loud Reference`, `Reset Detection State`, and `Restart`

No new maintenance buttons were added because the existing ESPHome buttons already cover the current maintenance needs.

## Package install steps

1. Enable Home Assistant packages if you do not already use them.
2. Copy `home_assistant/sentinel_helpers_package.yaml` into your HA config packages directory.
3. Restart Home Assistant or reload YAML-based helpers and automations.
4. Confirm the new helpers appear.

Example `configuration.yaml` snippet:

```yaml
homeassistant:
  packages: !include_dir_named packages
```

## Helpers created by the package

Counters:

- `counter.sentinel_motion_events`
- `counter.sentinel_sound_events`
- `counter.sentinel_smoke_events`
- `counter.sentinel_door_events`

Last-event helpers:

- `input_text.sentinel_last_event`
- `input_datetime.sentinel_last_event_time`
- `sensor.sentinel_last_event_summary`

Mode helper:

- `input_select.sentinel_mode`

## Default mode mapping

The example mode selector drives the existing ESPHome switches with this mapping:

- `Disarmed`: PIR off, sound off, smoke off
- `Motion Only`: PIR on, sound off, smoke off
- `Motion + Sound`: PIR on, sound on, smoke off
- `Full Monitor`: PIR on, sound on, smoke on

If that mapping is too aggressive or too weak, edit the `choose:` block in `home_assistant/sentinel_helpers_package.yaml`.

## Entity ID assumptions

The example package assumes the default ESPHome entity IDs created from the current firmware, including:

- `binary_sensor.sentinel_motion`
- `binary_sensor.sentinel_sound_intrusion`
- `binary_sensor.sentinel_smoke_alarm_detected`
- `binary_sensor.sentinel_door`
- `switch.sentinel_enable_pir_sensing`
- `switch.sentinel_enable_sound_sensing`
- `switch.sentinel_enable_smoke_alarm_sensing`

If your Home Assistant instance renamed any of those, update the package before enabling it.

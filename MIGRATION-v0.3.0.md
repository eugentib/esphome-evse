# Migrating from v0.2.x to v0.3.0

v0.3.0 intentionally changes the contactor configuration so the dedicated EVSE FreeRTOS task can control the relay GPIO directly.

## Remove the old internal GPIO switch

Remove:

```yaml
switch:
  - platform: gpio
    id: evse_contactor_hw
    pin: GPIO16
    restore_mode: ALWAYS_OFF
    internal: true
```

Keep the two template controls `EVSE Enable` and `EVSE Available`.

## Change the EVSE contactor setting

Old:

```yaml
evse:
  contactor: evse_contactor_hw
```

New:

```yaml
evse:
  contactor_pin: GPIO16
```

If your relay is active-low, use the normal ESPHome pin schema:

```yaml
evse:
  contactor_pin:
    number: GPIO16
    inverted: true
```

## Optional Bluetooth proxy

The supplied example adds:

```yaml
esp32_ble_tracker:
  software_coexistence: true
  scan_parameters:
    active: false

bluetooth_proxy:
  active: false
```

## EVSE task defaults

```yaml
evse:
  sample_interval: 20ms
  sample_window_us: 1600
  task_core: 1
  task_priority: 5
  task_stack_size: 4096
```

The Home Assistant controls keep the same names and behavior.

# Migration to v0.4.8

No pin or EVSE-state YAML changes are required from v0.4.7.

The component now requires an ESPHome OTA component:

```yaml
ota:
  - platform: esphome
```

The OTA safety interlock is internal to the EVSE component; no `on_begin`
automation is required.

At OTA start the contactor is opened immediately and CP is forced to -12 V.
If OTA fails/aborts, the EVSE resumes with Enable OFF.

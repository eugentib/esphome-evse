# Migration to v0.4.0

v0.4.0 changes the CP measurement configuration from raw ADC counts to calibrated millivolts.

## YAML changes

Replace:

```yaml
state_a_min_raw: ...
state_a_max_raw: ...
state_b_min_raw: ...
state_b_max_raw: ...
state_c_min_raw: ...
state_c_max_raw: ...
state_d_min_raw: ...
state_d_max_raw: ...
diode_min_raw: ...
diode_max_raw: ...

cp_high_raw:
  name: "EVSE CP High Raw"
cp_low_raw:
  name: "EVSE CP Low Raw"
```

with the initial values for the 470k / 100k / 100k feedback network:

```yaml
state_a_min_mv: 2550
state_a_max_mv: 2745
state_b_min_mv: 2260
state_b_max_mv: 2455
state_c_min_mv: 1970
state_c_max_mv: 2170
state_d_min_mv: 1680
state_d_max_mv: 1880
diode_min_mv: 240
diode_max_mv: 435

cp_high_mv:
  name: "EVSE CP High"
cp_low_mv:
  name: "EVSE CP Low"
```

Also change the recommended sampling window to:

```yaml
sample_window_us: 3000
```

## ADC configuration

No extra YAML is required. The component configures GPIO34 with `ADC_11db` and calls `analogReadMilliVolts()`.

## Important

The mV values above are theoretical starting values for:

- R9 = 470 kΩ from CP,
- R10 = 100 kΩ to 3.3 V,
- R11 = 100 kΩ to GND.

Verify actual A/B/C/D and negative-half-cycle readings on the assembled board before connecting a vehicle.

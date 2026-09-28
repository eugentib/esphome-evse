# Migration to v0.5.1

No existing v0.5.0 setting needs to change.

To expose the new energy sensors:

```yaml
session_energy:
  name: "EVSE Session Energy"

total_energy:
  name: "EVSE Total Energy"
```

Both require `ct_adc_pin`.

`EVSE Total Energy` is reported in kWh with `device_class: energy` and
`state_class: total_increasing`, so Home Assistant can use it directly in the
Energy Dashboard.

The energy remains estimated from CT RMS current using `ct_nominal_voltage`
(230 V in the supplied example) and assumes PF approximately 1.

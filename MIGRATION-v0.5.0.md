# Migration to v0.5.0

The CT feature is optional. Existing v0.4.9 configurations continue to work
without a CT.

For the current hardware add:

```yaml
evse:
  # existing CP ADC
  pilot_adc_pin: GPIO34

  # CT 2000:1 with 75 ohm burden
  ct_adc_pin: GPIO32
  ct_ratio: 2000
  ct_burden_ohms: 75
  ct_nominal_voltage: 230
  ct_rms_window: 200ms
  ct_noise_floor: 0.15

  charging_current:
    name: "EVSE Measured Charging Current"

  charging_power:
    name: "EVSE Estimated Charging Power"
```

GPIO32 and GPIO34 are both on ADC1. The component configures one continuous
ADC/DMA stream with two conversion patterns, so no second ESPHome `adc:`
component should be configured for GPIO32.

With the default 80 kS/s aggregate sample rate, each channel receives
approximately 40 kS/s.

Hardware assumptions for the example:

```text
CT secondary -> 75 ohm burden
one CT/burden side -> 1.65 V bias
other CT/burden side -> GPIO32 through the chosen ADC protection/series resistor
bias divider -> 10k / 10k from 3.3 V
bias decoupling -> 10 uF + 100 nF
```

At 20 A primary, a 2000:1 CT produces 10 mA RMS secondary current. Across
75 ohm this is approximately 0.75 V RMS (1.06 V peak), which fits comfortably
around a 1.65 V ADC bias.

`charging_power` is estimated from `230 V * measured_current` and assumes
power factor approximately 1.

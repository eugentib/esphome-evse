# Migration to v0.4.2

v0.4.2 changes CP acquisition from repeated one-shot conversions to continuous ADC/DMA.

## Replace the old sampling option

`sample_window_us` is still accepted for v0.4.1 YAML compatibility but is ignored by the DMA sampler. You may remove it.

Recommended configuration:

```yaml
sample_interval: 20ms
adc_sample_rate: 80000
adc_peak_samples: 16
adc_min_samples: 200
adc_fault_time: 100ms
cp_confirm_windows: 3
stable_time: 250ms
```

## Feedback network used by the current hardware

The supplied example now assumes:

```text
R9  = 500 kΩ from CP
R10 = 100 kΩ to 3.3 V
R11 = 100 kΩ to GND
```

with initial windows:

```yaml
state_a_min_mv: 2480
state_a_max_mv: 2700
state_b_min_mv: 2210
state_b_max_mv: 2430
state_c_min_mv: 1935
state_c_max_mv: 2160
state_d_min_mv: 1660
state_d_max_mv: 1885
diode_min_mv: 300
diode_max_mv: 520
```

## Optional diagnostics

When `timing_debug: true`, add:

```yaml
adc_sample_count:
  name: "EVSE ADC Samples Last Cycle"
adc_read_errors:
  name: "EVSE ADC Read Errors"
```

At 80 kS/s and a 20 ms control interval, `ADC Samples Last Cycle` should normally be of the order of 1600 samples, with some variation from scheduling and DMA frame boundaries.

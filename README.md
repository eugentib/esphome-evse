# esphome-evse

Experimental IEC 61851 / J1772 AC EVSE controller implemented as an ESPHome external component.

**Version: v0.1.1**

## v0.1.1
- `pilot_adc_pin: GPIO34` now works using ESPHome's GPIO schema.
- B1/B2 and C1/C2 are reported separately.
- State A and B1 use DC CP; PWM starts only in B when EVSE is enabled.
- Contactor is allowed only in stable State C.

## Pinout (ESP32 Relay X2)
```text
GPIO16 -> onboard relay 1 -> external contactor coil
GPIO17 -> reserved
GPIO25 -> 1 kHz CP PWM logic output
GPIO34 -> protected/scaled CP feedback ADC
```

## State sequence
```text
A  : +12 V DC, disconnected
B1 : ~+9 V DC, vehicle connected, EVSE not ready
B2 : +9/-12 V PWM, EVSE ready
C1 : +6/-12 V PWM, charge requested, contactor off
C2 : +6/-12 V PWM, contactor on
D  : ventilation requested -> refused
E  : CP fault
```

## GitHub use
```yaml
external_components:
  - source: github://eugentib/esphome-evse@main
    components: [evse]
    refresh: 1min
```

The ADC thresholds in the example are placeholders. Calibrate them on the final CP analog front-end before connecting a vehicle.

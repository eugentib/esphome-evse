# esphome-evse

Experimental IEC 61851 / J1772 AC EVSE controller implemented as an ESPHome external component.

> **Status: v0.1.0 / experimental**
>
> Do not treat this repository as a certified EVSE safety controller.

## Current scope

- classic ESP32 / ESP32 Relay X2
- ESPHome + Arduino framework
- single phase
- fixed Type 2 cable
- 6...16 A example
- 1 kHz CP PWM
- CP high/low ADC sampling
- A/B/C/D/E state classification
- negative CP half-cycle / diode plausibility check
- external contactor command
- native Home Assistant entities

## Install from GitHub

After pushing this repository to your GitHub account:

```yaml
external_components:
  - source: github://eugentib/esphome-evse@main
    components: [evse]
    refresh: 5min
```

For a stable installation, pin a tag:

```yaml
external_components:
  - source: github://eugentib/esphome-evse@v0.1.0
    components: [evse]
    refresh: never
```

## ESP32 Relay X2 example

See `examples/esp32-relay-x2.yaml`.

Current example pinout:

```text
GPIO16 -> onboard relay 1 -> external contactor coil
GPIO25 -> 1 kHz logic PWM -> external +/-12 V CP driver
GPIO34 -> protected/scaled CP feedback -> ADC
```

The onboard relay must not carry EV charging current.

## Home Assistant

The example exposes:

- EVSE Enable
- EVSE Current Limit
- EVSE State
- EVSE CP High Raw
- EVSE CP Low Raw
- EVSE Advertised Current
- EVSE Vehicle Connected
- EVSE Charging
- EVSE Fault

No MQTT broker is required when using the native ESPHome API.

## Analog CP interface

The firmware expects an external front-end:

```text
ESP32 PWM 1 kHz -> CP driver -> +12/-12 V -> 1 kΩ series -> Type 2 CP

Type 2 CP -> protected bipolar scaling/level shifting -> ESP32 ADC 0..3.3 V
```

The feedback circuit must preserve both the positive and negative CP levels.

Expected connector-side values are approximately:

| State | CP positive | CP negative |
|---|---:|---:|
| A | +12 V | -12 V |
| B | +9 V | -12 V |
| C | +6 V | -12 V |
| D | +3 V | -12 V |

State D is deliberately refused by this implementation.

## Current advertisement

For the normal 6...51 A region:

```text
Imax[A] = duty[%] * 0.6
```

Examples:

| Current | Duty |
|---:|---:|
| 6 A | 10.0% |
| 10 A | 16.7% |
| 16 A | 26.7% |

## Calibration

The raw thresholds in the example are placeholders. Bench-calibrate A/B/C/D and the negative diode-check level before connecting a vehicle.

## Safety work still required

Before real use, add/validate:

- residual-current / RDC-DD fault input
- contactor auxiliary feedback
- welded-contactor detection
- over-temperature inputs
- correct protective devices
- protective earth continuity
- safe mains/SELV separation
- suitable enclosure and wiring

Home Assistant must not be part of the safety chain.

## Development

```bash
esphome config examples/esp32-relay-x2.yaml
esphome compile examples/esp32-relay-x2.yaml
```

## License

MIT.

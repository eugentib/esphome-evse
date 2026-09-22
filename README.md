# esphome-evse

Experimental IEC 61851 / SAE J1772 basic-signaling EVSE controller implemented as an ESPHome external component.

**Current version: v0.2.1**  
**Target:** classic ESP32 / ESP32 Relay X2, single phase, fixed Type 2 cable.

> This is experimental firmware for a DIY EVSE. It is not a certified safety controller. Mains protection, residual-current protection, PE integrity, contactor supervision, thermal protection and the analog CP interface remain hardware responsibilities and must be engineered/tested independently.

## v0.2.1 highlights

- Explicit EVSE states: A, B1, B2, C1, C2, D1, D2, E and F.
- State `1` means energy is not offered: CP held at +12 V DC and contactor open (except during graceful stop).
- State `2` means energy is offered: 1 kHz CP PWM; C2/D2 energize the contactor.
- `Enable = OFF` while charging performs graceful stop:
  1. C2/D2 -> C1/D1.
  2. PWM is suppressed; CP goes to +12 V DC.
  3. The contactor is kept closed while the EV winds current down.
  4. If EV returns to B/A, the contactor opens immediately.
  5. Otherwise it is forced open after 6 seconds.
- `Available = OFF` enters State F and drives CP to -12 V DC.
- Pilot/diode faults enter State E and drive CP to -12 V DC.
- Transient pilot/diode faults retry after 60 seconds, or can be reset manually.
- A/B/C/D are detected using explicit ADC windows with invalid gaps.
- Diode checking uses a bounded raw ADC window for the negative CP half-cycle, not just one threshold.
- State D can be accepted only when `allow_ventilation: true`; default is false.

The state semantics follow the same IEC/J1772 model used by the ESP32-EVSE reference project: B1/C1/D1 suppress PWM; B2/C2/D2 offer energy; leaving C2/D2 for a paused state raises CP to steady +12 V before opening the contactor.

## GitHub install

During development:

```yaml
external_components:
  - source: github://eugentib/esphome-evse@main
    components: [evse]
    refresh: 1min
```

After tagging v0.2.1:

```yaml
external_components:
  - source: github://eugentib/esphome-evse@v0.2.1
    components: [evse]
    refresh: never
```

## ESP32 Relay X2 pin assignment

The example uses:

```text
GPIO16 -> onboard relay 1 -> EXTERNAL contactor coil
GPIO17 -> onboard relay 2 -> reserved
GPIO25 -> 1 kHz logic output -> external bipolar CP driver
GPIO34 -> protected/scaled CP feedback -> ADC
```

The onboard relay does **not** carry EV charging current.

## Required CP driver behavior

```text
GPIO25 high / 100% duty -> CP +12 V
GPIO25 low  /   0% duty -> CP -12 V
GPIO25 PWM @ 1 kHz      -> CP +12 V / -12 V
```

The Type 2 CP path still requires the IEC/J1772 series resistance and a correctly engineered analog front-end.

## CP feedback

The ADC front-end must preserve both CP polarities while keeping GPIO34 within 0..3.3 V.

The example raw windows are **placeholders**, based approximately on the earlier proposed mapping:

```text
CP -12 V -> ADC ~0.3 V
CP   0 V -> ADC ~1.65 V
CP  +3 V -> ADC ~1.99 V
CP  +6 V -> ADC ~2.33 V
CP  +9 V -> ADC ~2.67 V
CP +12 V -> ADC ~3.01 V
```

Calibrate the real hardware before connecting a vehicle.

## State sequence

```text
A    +12 V DC       contactor open       no vehicle
 |
 +--> B1  +9 V DC   contactor open       vehicle connected, paused
       |
       | Enable ON
       v
      B2  +9/-12 PWM contactor open      energy offered
       |
       | EV requests energy
       v
      C2  +6/-12 PWM contactor closed    charging
       |
       | Enable OFF / pause
       v
      C1  +6 V DC    contactor held temporarily
       |
       | EV winds down -> B
       v
      B1             contactor opens
```

If the EV does not wind down during C1/D1, the contactor is forced open after the configured graceful-stop timeout (default 6 s).

## Home Assistant entities

The example exposes:

- EVSE Enable
- EVSE Available
- EVSE Reset Fault
- EVSE Current Limit
- EVSE State
- EVSE CP Physical State
- EVSE Fault Reason
- EVSE CP High Raw
- EVSE CP Low Raw
- EVSE Advertised Current
- EVSE Vehicle Connected
- EVSE Charging
- EVSE Graceful Stop
- EVSE Fault

## Safety items still intentionally outside v0.2.1

Before real charging, add and test at least:

- RCD/RDC-DD input and self-test behavior;
- contactor auxiliary-contact feedback;
- welded-contactor detection;
- temperature monitoring;
- PE/earth integrity strategy;
- over-current protection;
- correctly rated cabling, terminals, contactor and enclosure.

Home Assistant is not in the safety chain.

## Example

Use `examples/esp32-relay-x2.yaml` in Home Assistant/ESPHome.

`examples/esp32-relay-x2-ci.yaml` is the same configuration but loads the component locally so GitHub Actions validates the commit being tested rather than an older remote `main`.

## License

MIT.

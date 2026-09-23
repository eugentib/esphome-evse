# esphome-evse

Experimental IEC 61851 / SAE J1772 basic-signaling EVSE controller implemented as an ESPHome external component.

**Current version: v0.4.1**  
**Target:** classic dual-core ESP32 / ESP32 Relay X2, single phase, fixed Type 2 cable.

> Experimental DIY EVSE firmware. It is not a certified safety controller. Mains protection, residual-current protection, PE integrity, contactor supervision, thermal protection and the analog CP interface remain hardware responsibilities and must be engineered/tested independently.

## v0.4.1 timing diagnostics

v0.4.1 adds optional timing instrumentation for the dedicated EVSE task. It is controlled by one flag:

```yaml
timing_debug: true
```

With the flag enabled, the example exposes:

```yaml
task_late_cycles:
  name: "EVSE Task Late Cycles"
task_last_runtime:
  name: "EVSE Task Last Runtime"
task_max_runtime:
  name: "EVSE Task Max Runtime"
task_last_lateness:
  name: "EVSE Task Last Lateness"
task_max_lateness:
  name: "EVSE Task Max Lateness"
task_missed_deadlines:
  name: "EVSE Task Missed Deadlines"
```

For production, change only:

```yaml
timing_debug: false
```

The timing sensor definitions may remain in YAML; the component will not instantiate or publish them.

Definitions:

- **Late Cycles**: cycle started more than 1 ms after its nominal release time.
- **Last/Max Runtime**: execution time of the EVSE control cycle.
- **Last/Max Lateness**: positive delay between nominal and actual task start.
- **Missed Deadlines**: cycle finished at or after the next nominal 20 ms release point.

`EVSE Task Running` remains independent of `timing_debug`, because it is a useful operational health signal rather than development instrumentation.

If `timing_debug` is omitted, v0.4.1 keeps backward compatibility with v0.4.0: the presence of the old `task_late_cycles` or `task_max_runtime` entities automatically enables timing instrumentation.

## v0.4.0 calibrated CP measurement

v0.4.0 changes CP measurement from uncalibrated 12-bit ADC counts to calibrated millivolts:

- explicitly configures GPIO34 to `ADC_11db`;
- samples CP with `analogReadMilliVolts()`;
- state windows are configured in mV;
- Home Assistant exposes `EVSE CP High` / `EVSE CP Low` in mV;
- default windows are calculated for the selected feedback network:
  - R9 = 470 kΩ from CP,
  - R10 = 100 kΩ to 3.3 V,
  - R11 = 100 kΩ to GND;
- sample window increased from 1.6 ms to 3.0 ms to give calibrated one-shot sampling enough time to observe the 100 µs positive CP pulse at the 6 A minimum duty cycle.

The nominal feedback transfer is:

```text
VADC ≈ 1.49135 V + 0.096154 × VCP
```

which gives approximately:

| CP | ADC |
|---:|---:|
| -12 V | 338 mV |
| 0 V | 1491 mV |
| +3 V | 1780 mV |
| +6 V | 2068 mV |
| +9 V | 2357 mV |
| +12 V | 2645 mV |

The initial state windows are:

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
```

These are theoretical starting values. Verify them on the assembled analog front-end before connecting a vehicle.

## Architecture retained from v0.3.0

```text
Dedicated FreeRTOS task (default Core 1, priority 5)
  -> calibrated CP ADC sampling
  -> CP A/B/C/D classification
  -> diode supervision
  -> IEC state machine
  -> CP duty/mode updates
  -> direct contactor GPIO control

ESPHome main loop
  -> Home Assistant API
  -> BLE proxy
  -> Wi-Fi / logging
  -> entity publication only
```

The task uses `vTaskDelayUntil()` for periodic execution. The default control period is 20 ms and the ADC acquisition window is 3 ms.

## GitHub install

During development:

```yaml
external_components:
  - source: github://eugentib/esphome-evse@main
    components: [evse]
    refresh: 1min
```

After tagging v0.4.1:

```yaml
external_components:
  - source: github://eugentib/esphome-evse@v0.4.1
    components: [evse]
    refresh: never
```

## ESP32 Relay X2 pin assignment

```text
GPIO16 -> onboard relay 1 -> EXTERNAL contactor coil
GPIO17 -> onboard relay 2 -> reserved
GPIO25 -> 1 kHz logic output -> external bipolar CP driver
GPIO34 -> protected/scaled CP feedback -> ADC1
```

## Bluetooth proxy

The example keeps the conservative passive/adverts-only configuration:

```yaml
esp32_ble_tracker:
  software_coexistence: true
  scan_parameters:
    active: false

bluetooth_proxy:
  active: false
```

The EVSE control path remains in its own pinned FreeRTOS task.

## State model

- A — disconnected
- B1 — connected, energy not offered
- B2 — connected, PWM active
- C1 — charge requested, paused/graceful stop
- C2 — charging
- D1/D2 — ventilation states
- E — internal EVSE/pilot error state
- F — unavailable

`Enable = OFF` while charging performs graceful stop before the contactor is forced open.

## CP feedback hardware

The v0.4.x defaults assume:

```text
CP ---- 470k ----+
                 |
3.3V -- 100k ----+---- 1k ---- GPIO34
                 |               |
GND --- 100k ----+             470pF
                                 |
                                GND
```

Use correctly oriented rail clamps at the ADC pin. The ESP32 pin must never be exposed directly to CP.

Because the offset is derived from the board's 3.3 V rail and resistor tolerances are finite, final bench verification remains required even though the ESP32 ADC conversion itself is calibrated to millivolts.

## Safety items still outside v0.4.1

Before real charging, add and test at least:

- RCD/RDC-DD input and self-test behavior;
- contactor auxiliary-contact feedback;
- welded-contactor detection;
- temperature monitoring;
- PE/earth integrity strategy;
- over-current protection;
- correctly rated cabling, terminals, contactor and enclosure.

Home Assistant and Bluetooth are not in the safety chain.

## License

MIT.

# esphome-evse

Experimental IEC 61851 / SAE J1772 basic-signaling EVSE controller implemented as an ESPHome external component.

**Current version: v0.4.8**  
**Framework: native ESP-IDF**  
**Target:** classic dual-core ESP32 / ESP32 Relay X2, single phase, fixed Type 2 cable.

> Experimental DIY EVSE firmware. It is not a certified safety controller.

## v0.4.8

### OTA safety interlock

`on_shutdown()` is not sufficient for an EVSE because an OTA transfer can run
for seconds before application shutdown/reboot.

v0.4.8 subscribes directly to ESPHome's global OTA state listener. On
`OTA_STARTED`, before the blocking transfer:

```text
OTA lockout ON
-> Enable forced OFF
-> EVSE task suspended
-> contactor GPIO forced OFF immediately
-> CP forced to -12 V
```

The state machine and `close_contactor_()` both independently check the atomic
OTA lockout, so a concurrent task cannot re-close the relay.

After successful OTA, the safe state is retained until reboot.

If OTA is aborted or fails, the task resumes with `Enable = OFF` and
`Available = ON`; charging therefore requires a new explicit Enable command.

## v0.4.7

### LEDC log warning fix

The native LEDC initialization log now uses `PRIu32` for the 32-bit frequency
values, removing the ESP-IDF compiler warning about `%u` versus `uint32_t`.

### Wi-Fi migration / provisioning

The normal example now uses ESPHome's multi-network syntax and a fallback AP
with captive portal:

```yaml
wifi:
  networks:
    - ssid: !secret wifi_ssid
      password: !secret wifi_password

    # Optional additional known network:
    # - ssid: !secret wifi_ssid_2
    #   password: !secret wifi_password_2

  ap:
    ssid: "${name}-setup"
    password: !secret wifi_fallback_password

captive_portal:
```

Add this to `secrets.yaml`:

```yaml
wifi_fallback_password: "choose-a-strong-password"
```

When no configured station network is reachable, ESPHome starts the fallback
AP. Connecting to it opens a captive portal where a different Wi-Fi SSID and
password can be entered and saved on the device.

For permanent reproducible configuration, also add any long-term secondary
network to the `networks:` list / `secrets.yaml`.

## v0.4.6

Fix CP PWM duty updates while charging.

In v0.4.5, changing the advertised current while PWM was already running used
the thread-safe LEDC duty-update helper. That API can fail through LEDC's
fade-related internal path, and the EVSE correctly treated the failure as a CP
output fault.

Because the EVSE task is the sole owner of its LEDC channel, v0.4.6 uses:

```text
ledc_set_duty()
ledc_update_duty()
```

for in-place duty changes. Starting PWM from a static state still uses
`ledc_channel_config()`.

Any genuine LEDC error remains fail-closed.

## v0.4.5

v0.4.5 changes how voltages between the valid A/B/C/D windows are handled.

Normal physical transitions cross intermediate voltages. Those samples are now
treated as a short transition gap rather than becoming a stable `Invalid` CP state.

```yaml
stable_time: 250ms
invalid_grace_time: 100ms
```

Behavior:

```text
B -> Invalid for 20...80 ms -> A
     ^ ignored transition gap

B -> Invalid continuously for >=100 ms
     ^ CP voltage fault
```

During the grace interval the last valid stable CP state is retained. Invalid
samples reset any pending valid-state candidate, so a new A/B/C/D level still
has to satisfy `cp_confirm_windows` and `stable_time`.

ADC/DMA acquisition faults, diode supervision and CP output failures remain
independent and are not hidden by `invalid_grace_time`.

The supplied example keeps small invalid guard bands. In particular:

```yaml
state_b_max_mv: 2430
state_a_min_mv: 2440
```

so a real intermediate voltage can still be recognized as abnormal if it
persists, while a normal fast B/A transition does not trip the EVSE.

## v0.4.4

Build-system fix for native ESP-IDF:

- explicitly re-enables ESP-IDF `esp_adc`, which ESPHome excludes by default;
- explicitly requests `esp_driver_ledc` used by the native CP PWM path;
- no EVSE state-machine, PWM-frequency or ADC-sampling behavior changes relative to v0.4.3.

This fixes the ESP-IDF error reporting that `esp_adc/adc_continuous.h` is provided by
`esp_adc` but that `esp_adc` is missing from the generated `src` component requirements.

## v0.4.3

v0.4.3 moves the EVSE low-level path fully to native ESP-IDF.

### Native CP PWM

The component now owns GPIO25 directly. Remove the ESPHome `output: ledc` component and use:

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf

evse:
  pilot_pwm_pin: GPIO25
  pilot_adc_pin: GPIO34
```

The CP logic output uses the ESP-IDF LEDC peripheral directly at exactly 1 kHz.

- `POSITIVE_DC`: LEDC is stopped with idle HIGH -> true static +12 V through the external driver.
- `PWM`: the LEDC channel is explicitly configured/restarted at 1 kHz with the IEC duty cycle.
- `NEGATIVE_DC`: LEDC is stopped with idle LOW -> true static -12 V through the external driver.

The code deliberately reconfigures the LEDC channel when moving from a static state into PWM instead of depending on implicit resume behavior after `ledc_stop()`.

At 6 A the requested duty is 10%; with the 10-bit LEDC timer the applied duty is about 9.96%.

### CP acquisition

The continuous ADC/DMA sampler from v0.4.2 is retained:

- GPIO34 / ADC1;
- 80 ksample/s default;
- approximately 80 samples per 1 kHz CP period;
- approximately 8 samples during the minimum 100 us positive pulse at 6 A;
- top/bottom sample groups are averaged instead of trusting one peak sample;
- raw extremes are converted to calibrated mV with the ESP-IDF ADC calibration driver.

### Development diagnostics

With:

```yaml
timing_debug: true
```

the supplied example also exposes:

- EVSE ADC Samples Last Cycle
- EVSE ADC Read Errors
- EVSE Pilot Duty
- EVSE Pilot Mode
- EVSE Task timing diagnostics

For the final installation set only:

```yaml
timing_debug: false
```

and these development entities are not instantiated.

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

The task uses `vTaskDelayUntil()` for periodic execution. The default control period is 20 ms. In v0.4.3 the ADC runs continuously in DMA mode between task executions; the task drains and evaluates the accumulated waveform samples each cycle.

## GitHub install

During development:

```yaml
external_components:
  - source: github://eugentib/esphome-evse@main
    components: [evse]
    refresh: 1min
```

After tagging v0.4.3:

```yaml
external_components:
  - source: github://eugentib/esphome-evse@v0.4.8
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

The v0.4.3 example assumes:

```text
CP ---- 500k ----+
                 |
3.3V -- 100k ----+---- 1k ---- GPIO34
                 |               |
GND --- 100k ----+             470pF
                                 |
                                GND
```

Use correctly oriented rail clamps at the ADC pin. The ESP32 pin must never be exposed directly to CP.

Because the offset is derived from the board's 3.3 V rail and resistor tolerances are finite, final bench verification remains required even though the ESP32 ADC conversion itself is calibrated to millivolts.

## Safety items still outside v0.4.3

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

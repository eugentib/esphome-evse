# Migration to v0.4.3

v0.4.3 switches the example to native ESP-IDF and makes the EVSE component own the CP PWM pin directly.

## 1. Framework

Change:

```yaml
esp32:
  board: esp32dev
  framework:
    type: arduino
```

to:

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf
```

## 2. Remove the old LEDC output block

Delete:

```yaml
output:
  - platform: ledc
    id: cp_pwm
    pin: GPIO25
    frequency: 1000Hz
```

## 3. Change the EVSE CP output configuration

Replace:

```yaml
evse:
  pilot_output: cp_pwm
```

with:

```yaml
evse:
  pilot_pwm_pin: GPIO25
```

The rest of the v0.4.2 ADC/DMA configuration can remain unchanged.

## 4. Optional development diagnostics

With `timing_debug: true` you may keep:

```yaml
pilot_duty:
  name: "EVSE Pilot Duty"
pilot_mode:
  name: "EVSE Pilot Mode"
```

Set `timing_debug: false` for the final installation and the development entities are not instantiated.

## PWM behavior

v0.4.3 does not use an ESPHome FloatOutput for CP.

- A/B1/C1 -> GPIO25 static HIGH.
- B2/C2/D2 -> native 1 kHz ESP-IDF LEDC PWM.
- E/F -> GPIO25 static LOW (current one-bit driver fail-safe behavior).

When PWM starts after a static state, the LEDC channel is explicitly configured again so PWM start does not depend on undocumented/implicit resume behavior.

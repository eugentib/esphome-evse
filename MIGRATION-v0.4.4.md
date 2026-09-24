# Migration to v0.4.4

No YAML changes are required from v0.4.3.

v0.4.4 only fixes ESP-IDF component dependencies required by the native ADC/DMA and LEDC implementation.

Keep:

```yaml
esp32:
  board: esp32dev
  framework:
    type: esp-idf

evse:
  pilot_pwm_pin: GPIO25
  pilot_adc_pin: GPIO34
```

The generated build now explicitly includes `esp_adc`; ESPHome excludes that component by default unless requested.

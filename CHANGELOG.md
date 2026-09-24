# Changelog

## v0.4.3

- Switch ESPHome example framework from Arduino to native ESP-IDF.
- Remove ESPHome `output: ledc` dependency from the EVSE CP path.
- EVSE component now owns GPIO25 directly through ESP-IDF LEDC at 1 kHz.
- Use true static HIGH/LOW for CP DC states with `ledc_stop(..., idle_level)`.
- Explicitly restart/reconfigure the LEDC channel when entering PWM.
- Use `ledc_set_duty_and_update()` for duty updates while PWM is already active.
- Add optional `EVSE Pilot Mode` and `EVSE Pilot Duty` development diagnostics.
- Retain continuous ADC/DMA CP acquisition from v0.4.2.
- Replace Arduino `millis()/micros()` usage with `esp_timer_get_time()`.
- Add CP output hardware fault handling that prevents contactor energization.

## v0.4.2

- Replace one-shot `analogReadMilliVolts()` CP acquisition with ESP-IDF ADC continuous/DMA mode on ADC1.
- Default ADC sampling rate: 80 kS/s, giving ~8 samples in the 100 us positive pulse at 6 A / 10% duty.
- Estimate CP high/low from averages of the 16 highest/lowest raw samples rather than a single extreme.
- Convert only the robust raw extrema to calibrated mV via `adc_cali_raw_to_voltage()`.
- Add ADC acquisition supervision and a dedicated ADC fault.
- Add `cp_confirm_windows` consecutive-window validation for physical CP state changes.
- Add optional ADC sample-count and read-error diagnostics under `timing_debug`.
- Update example thresholds for the assembled 500k / 100k / 100k feedback network.

## v0.4.1

- Add `timing_debug` switch for development-only EVSE task instrumentation.
- Add Task Last Runtime, Last Lateness, Max Lateness and Missed Deadlines sensors.
- Define Late Cycles as task starts more than 1 ms after nominal release.
- Define Missed Deadlines as cycles that complete at/after the next nominal release.
- Keep `EVSE Task Running` outside timing debug as an operational health signal.
- Preserve v0.4.0 YAML compatibility: existing timing sensors auto-enable instrumentation when `timing_debug` is omitted.
- When `timing_debug: false`, timing sensor definitions may remain in YAML without being instantiated.

## v0.4.0

- Switch CP state thresholds from raw ADC counts to calibrated millivolts.
- Configure CP ADC explicitly for `ADC_11db`.
- Use `analogReadMilliVolts()` in the dedicated EVSE task.
- Rename CP telemetry to `cp_high_mv` / `cp_low_mv`.
- Add initial mV windows for R9=470k, R10=100k, R11=100k feedback network.
- Increase default CP acquisition window to 3000 us.
- Keep the dedicated FreeRTOS task and passive Bluetooth proxy architecture from v0.3.0.

## v0.3.0

- Move CP sampling, diode supervision and EVSE state machine into a dedicated FreeRTOS task.
- Pin EVSE task to a configurable ESP32 core (default Core 1).
- Add configurable task priority and stack size.
- Use `vTaskDelayUntil()` for periodic scheduling.
- Change contactor interface from an ESPHome `switch` to direct `contactor_pin` GPIO control.
- Keep ESPHome `loop()` limited to Home Assistant/entity publication.
- Add thread-safe request/status handoff between ESPHome and EVSE task.
- Add `EVSE Task Running`, `EVSE Task Late Cycles`, and `EVSE Task Max Runtime` diagnostics.
- Add passive/adverts-only Bluetooth proxy to ESP32 Relay X2 example.
- Keep CP ADC on ADC1/GPIO34.
- Document current State-E electrical-output limitation of the one-bit CP driver.

## v0.2.1

- Fix portable `uint32_t` log formatting.

## v0.2.0

- Add explicit A/B1/B2/C1/C2/D1/D2/E/F state machine.
- Add CP voltage windows, diode window and graceful stop.

# Changelog

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

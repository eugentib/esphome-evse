# Changelog

## v0.2.1

- Fix portable logging format for uint32_t timing values (PRIu32).
- No EVSE state-machine behavior changes from v0.2.0.

## v0.2.0

- Reworked state machine around IEC 61851 / J1772 A/B1/B2/C1/C2/D1/D2/E/F semantics.
- Added graceful stop C2/D2 -> C1/D1 with 6 s timeout.
- Added State F (`Available = OFF`) with steady negative pilot.
- Added State E output for pilot/diode faults.
- Added explicit A/B/C/D ADC windows with invalid gaps.
- Added bounded negative-half-cycle diode window and debounce time.
- Added transient fault auto-retry and manual reset action.
- Added optional State D charging gate (`allow_ventilation`, false by default).
- Added physical CP state, fault reason and graceful-stop Home Assistant entities.
- Added separate CI example using the local external component.

## v0.1.1

- Accepted normal ESPHome GPIO syntax for `pilot_adc_pin`.
- Added B1/B2 reporting.

## v0.1.0

Initial experimental release.

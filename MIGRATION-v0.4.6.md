# Migration to v0.4.6

No YAML changes are required from v0.4.5.

v0.4.6 fixes a runtime LEDC fault that could occur when changing the current
limit while CP PWM was already active.

The EVSE task is the sole owner of the CP LEDC channel, so in-place duty updates
now use `ledc_set_duty()` followed by `ledc_update_duty()`.

All safety behavior remains unchanged: a real LEDC error still opens the
contactor and raises `CP PWM/LEDC output failure`.

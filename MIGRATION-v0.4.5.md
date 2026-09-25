# Migration to v0.4.5

No mandatory pin or framework changes are required from v0.4.4.

Add the new CP transition-grace setting:

```yaml
invalid_grace_time: 100ms
```

Recommended A/B thresholds for the current assembled feedback circuit:

```yaml
state_a_min_mv: 2440
state_a_max_mv: 2700
state_b_min_mv: 2210
state_b_max_mv: 2430
```

The 2431..2439 mV region is intentionally invalid, but it no longer causes an
immediate/stable Invalid state. It is ignored for up to `invalid_grace_time`.

If the CP voltage stays outside all A/B/C/D windows for at least 100 ms, the
EVSE still enters the CP-voltage fault state.

`stable_time` continues to apply only to valid A/B/C/D state changes.

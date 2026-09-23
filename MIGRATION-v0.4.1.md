# Migration to v0.4.1

v0.4.1 adds optional timing diagnostics for the dedicated EVSE FreeRTOS task.

## Recommended development configuration

Add:

```yaml
timing_debug: true
```

and optionally expose all timing sensors:

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

## Production

You do not need to delete the diagnostic YAML. Set:

```yaml
timing_debug: false
```

The timing sensor definitions may remain present but they will not be instantiated or published.

`EVSE Task Running` remains active because it is treated as an operational health sensor.

## Backward compatibility

A v0.4.0 configuration that contains `task_late_cycles` and/or
`task_max_runtime` but does not contain `timing_debug` continues to work:
v0.4.1 automatically enables timing instrumentation in that case.

## Metric semantics

- `Task Late Cycles`: start lateness > 1000 us.
- `Task Last Runtime`: runtime of the latest cycle.
- `Task Max Runtime`: maximum runtime since boot.
- `Task Last Lateness`: positive start lateness of the latest cycle.
- `Task Max Lateness`: maximum start lateness since boot.
- `Task Missed Deadlines`: number of cycles whose completion reached or passed the next nominal task release.

The nominal control period remains 20 ms in the supplied example.

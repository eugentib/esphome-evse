import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import sensor, binary_sensor, text_sensor
from esphome.const import CONF_ID

CODEOWNERS = []
DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor"]

CONF_PILOT_PWM_PIN = "pilot_pwm_pin"
CONF_PILOT_ADC_PIN = "pilot_adc_pin"
CONF_CONTACTOR_PIN = "contactor_pin"
CONF_MAX_CURRENT = "max_current"
CONF_DEFAULT_CURRENT = "default_current"
CONF_ALLOW_VENTILATION = "allow_ventilation"
CONF_SAMPLE_INTERVAL = "sample_interval"
CONF_SAMPLE_WINDOW_US = "sample_window_us"  # legacy/accepted for v0.4.1 compatibility
CONF_ADC_SAMPLE_RATE = "adc_sample_rate"
CONF_ADC_PEAK_SAMPLES = "adc_peak_samples"
CONF_ADC_MIN_SAMPLES = "adc_min_samples"
CONF_ADC_FAULT_TIME = "adc_fault_time"
CONF_CP_CONFIRM_WINDOWS = "cp_confirm_windows"
CONF_STABLE_TIME = "stable_time"
CONF_CONTACTOR_CLOSE_DELAY = "contactor_close_delay"
CONF_GRACEFUL_STOP_TIMEOUT = "graceful_stop_timeout"
CONF_DIODE_FAULT_TIME = "diode_fault_time"
CONF_FAULT_RETRY_TIME = "fault_retry_time"
CONF_TASK_CORE = "task_core"
CONF_TASK_PRIORITY = "task_priority"
CONF_TASK_STACK_SIZE = "task_stack_size"
CONF_TIMING_DEBUG = "timing_debug"

CONF_STATE_A_MIN_MV = "state_a_min_mv"
CONF_STATE_A_MAX_MV = "state_a_max_mv"
CONF_STATE_B_MIN_MV = "state_b_min_mv"
CONF_STATE_B_MAX_MV = "state_b_max_mv"
CONF_STATE_C_MIN_MV = "state_c_min_mv"
CONF_STATE_C_MAX_MV = "state_c_max_mv"
CONF_STATE_D_MIN_MV = "state_d_min_mv"
CONF_STATE_D_MAX_MV = "state_d_max_mv"
CONF_DIODE_MIN_MV = "diode_min_mv"
CONF_DIODE_MAX_MV = "diode_max_mv"

CONF_STATE = "state"
CONF_PHYSICAL_STATE = "physical_state"
CONF_FAULT_REASON = "fault_reason"
CONF_CP_HIGH_MV = "cp_high_mv"
CONF_CP_LOW_MV = "cp_low_mv"
CONF_ADVERTISED_CURRENT = "advertised_current"
CONF_TASK_LATE_CYCLES = "task_late_cycles"
CONF_TASK_MAX_RUNTIME = "task_max_runtime"
CONF_TASK_LAST_RUNTIME = "task_last_runtime"
CONF_TASK_LAST_LATENESS = "task_last_lateness"
CONF_TASK_MAX_LATENESS = "task_max_lateness"
CONF_TASK_MISSED_DEADLINES = "task_missed_deadlines"
CONF_ADC_SAMPLE_COUNT = "adc_sample_count"
CONF_ADC_READ_ERRORS = "adc_read_errors"
CONF_PILOT_DUTY = "pilot_duty"
CONF_PILOT_MODE = "pilot_mode"
CONF_VEHICLE_CONNECTED = "vehicle_connected"
CONF_CHARGING = "charging"
CONF_STOPPING = "stopping"
CONF_FAULT = "fault"
CONF_TASK_RUNNING = "task_running"

evse_ns = cg.esphome_ns.namespace("evse")
EVSEComponent = evse_ns.class_("EVSEComponent", cg.Component)

mv = cv.int_range(min=0, max=3300)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(EVSEComponent),
    cv.Required(CONF_PILOT_PWM_PIN): pins.internal_gpio_output_pin_schema,
    cv.Required(CONF_PILOT_ADC_PIN): pins.internal_gpio_input_pin_schema,
    cv.Required(CONF_CONTACTOR_PIN): pins.internal_gpio_output_pin_schema,

    cv.Optional(CONF_MAX_CURRENT, default=16.0): cv.float_range(min=6.0, max=32.0),
    cv.Optional(CONF_DEFAULT_CURRENT, default=6.0): cv.float_range(min=6.0, max=32.0),
    cv.Optional(CONF_ALLOW_VENTILATION, default=False): cv.boolean,

    cv.Optional(CONF_SAMPLE_INTERVAL, default="20ms"): cv.positive_time_period_milliseconds,
    # Legacy v0.4.1 option. Accepted but no longer used by the DMA sampler.
    cv.Optional(CONF_SAMPLE_WINDOW_US, default=3000): cv.int_range(min=1000, max=5000),
    cv.Optional(CONF_ADC_SAMPLE_RATE, default=80000): cv.int_range(min=20000, max=200000),
    cv.Optional(CONF_ADC_PEAK_SAMPLES, default=16): cv.int_range(min=1, max=32),
    cv.Optional(CONF_ADC_MIN_SAMPLES, default=200): cv.int_range(min=32, max=4000),
    cv.Optional(CONF_ADC_FAULT_TIME, default="100ms"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_CP_CONFIRM_WINDOWS, default=3): cv.int_range(min=2, max=20),
    cv.Optional(CONF_STABLE_TIME, default="250ms"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_CONTACTOR_CLOSE_DELAY, default="1ms"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_GRACEFUL_STOP_TIMEOUT, default="6s"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_DIODE_FAULT_TIME, default="100ms"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_FAULT_RETRY_TIME, default="60s"): cv.positive_time_period_milliseconds,

    cv.Optional(CONF_TASK_CORE, default=1): cv.int_range(min=0, max=1),
    cv.Optional(CONF_TASK_PRIORITY, default=5): cv.int_range(min=1, max=24),
    cv.Optional(CONF_TASK_STACK_SIZE, default=4096): cv.int_range(min=3072, max=16384),
    # Timing instrumentation is intended for development/validation.
    # If omitted, legacy v0.4.0 timing sensors still auto-enable it.
    cv.Optional(CONF_TIMING_DEBUG): cv.boolean,

    cv.Required(CONF_STATE_A_MIN_MV): mv,
    cv.Required(CONF_STATE_A_MAX_MV): mv,
    cv.Required(CONF_STATE_B_MIN_MV): mv,
    cv.Required(CONF_STATE_B_MAX_MV): mv,
    cv.Required(CONF_STATE_C_MIN_MV): mv,
    cv.Required(CONF_STATE_C_MAX_MV): mv,
    cv.Required(CONF_STATE_D_MIN_MV): mv,
    cv.Required(CONF_STATE_D_MAX_MV): mv,
    cv.Required(CONF_DIODE_MIN_MV): mv,
    cv.Required(CONF_DIODE_MAX_MV): mv,

    cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_PHYSICAL_STATE): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_FAULT_REASON): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_CP_HIGH_MV): sensor.sensor_schema(unit_of_measurement="mV", accuracy_decimals=0, icon="mdi:sine-wave"),
    cv.Optional(CONF_CP_LOW_MV): sensor.sensor_schema(unit_of_measurement="mV", accuracy_decimals=0, icon="mdi:sine-wave"),
    cv.Optional(CONF_ADVERTISED_CURRENT): sensor.sensor_schema(
        unit_of_measurement="A", accuracy_decimals=1, icon="mdi:current-ac"
    ),
    cv.Optional(CONF_TASK_LATE_CYCLES): sensor.sensor_schema(
        accuracy_decimals=0, icon="mdi:timer-alert-outline"
    ),
    cv.Optional(CONF_TASK_MAX_RUNTIME): sensor.sensor_schema(
        unit_of_measurement="us", accuracy_decimals=0, icon="mdi:timer-outline"
    ),
    cv.Optional(CONF_TASK_LAST_RUNTIME): sensor.sensor_schema(
        unit_of_measurement="us", accuracy_decimals=0, icon="mdi:timer-outline"
    ),
    cv.Optional(CONF_TASK_LAST_LATENESS): sensor.sensor_schema(
        unit_of_measurement="us", accuracy_decimals=0, icon="mdi:clock-alert-outline"
    ),
    cv.Optional(CONF_TASK_MAX_LATENESS): sensor.sensor_schema(
        unit_of_measurement="us", accuracy_decimals=0, icon="mdi:clock-alert-outline"
    ),
    cv.Optional(CONF_TASK_MISSED_DEADLINES): sensor.sensor_schema(
        accuracy_decimals=0, icon="mdi:timer-off-outline"
    ),
    cv.Optional(CONF_ADC_SAMPLE_COUNT): sensor.sensor_schema(
        accuracy_decimals=0, icon="mdi:chart-bell-curve-cumulative"
    ),
    cv.Optional(CONF_ADC_READ_ERRORS): sensor.sensor_schema(
        accuracy_decimals=0, icon="mdi:alert-circle-outline"
    ),
    cv.Optional(CONF_PILOT_DUTY): sensor.sensor_schema(
        unit_of_measurement="%", accuracy_decimals=1, icon="mdi:pulse"
    ),
    cv.Optional(CONF_PILOT_MODE): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_VEHICLE_CONNECTED): binary_sensor.binary_sensor_schema(),
    cv.Optional(CONF_CHARGING): binary_sensor.binary_sensor_schema(),
    cv.Optional(CONF_STOPPING): binary_sensor.binary_sensor_schema(),
    cv.Optional(CONF_FAULT): binary_sensor.binary_sensor_schema(),
    cv.Optional(CONF_TASK_RUNNING): binary_sensor.binary_sensor_schema(),
}).extend(cv.COMPONENT_SCHEMA)


def _validate(config):
    if config[CONF_DEFAULT_CURRENT] > config[CONF_MAX_CURRENT]:
        raise cv.Invalid("default_current cannot exceed max_current")

    windows = [
        ("D", config[CONF_STATE_D_MIN_MV], config[CONF_STATE_D_MAX_MV]),
        ("C", config[CONF_STATE_C_MIN_MV], config[CONF_STATE_C_MAX_MV]),
        ("B", config[CONF_STATE_B_MIN_MV], config[CONF_STATE_B_MAX_MV]),
        ("A", config[CONF_STATE_A_MIN_MV], config[CONF_STATE_A_MAX_MV]),
    ]
    for name, lo, hi in windows:
        if lo > hi:
            raise cv.Invalid(
                f"state_{name.lower()}_min_mv must be <= state_{name.lower()}_max_mv"
            )

    if not (
        config[CONF_STATE_D_MAX_MV] < config[CONF_STATE_C_MIN_MV]
        and config[CONF_STATE_C_MAX_MV] < config[CONF_STATE_B_MIN_MV]
        and config[CONF_STATE_B_MAX_MV] < config[CONF_STATE_A_MIN_MV]
    ):
        raise cv.Invalid(
            "CP A/B/C/D windows must be ordered, non-overlapping, and leave invalid gaps"
        )

    if config[CONF_DIODE_MIN_MV] > config[CONF_DIODE_MAX_MV]:
        raise cv.Invalid("diode_min_mv must be <= diode_max_mv")

    expected_samples = (config[CONF_ADC_SAMPLE_RATE] * config[CONF_SAMPLE_INTERVAL].total_milliseconds) // 1000
    if config[CONF_ADC_MIN_SAMPLES] >= expected_samples:
        raise cv.Invalid(
            "adc_min_samples must be lower than the nominal samples collected per task interval"
        )
    if config[CONF_ADC_PEAK_SAMPLES] * 2 >= config[CONF_ADC_MIN_SAMPLES]:
        raise cv.Invalid("adc_peak_samples is too large relative to adc_min_samples")

    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pilot_pwm_pin = await cg.gpio_pin_expression(config[CONF_PILOT_PWM_PIN])
    pilot_adc_pin = await cg.gpio_pin_expression(config[CONF_PILOT_ADC_PIN])
    contactor_pin = await cg.gpio_pin_expression(config[CONF_CONTACTOR_PIN])

    cg.add(var.set_pilot_pwm_pin(pilot_pwm_pin))
    cg.add(var.set_pilot_adc_pin(pilot_adc_pin))
    cg.add(var.set_contactor_pin(contactor_pin))
    cg.add(var.set_max_current(config[CONF_MAX_CURRENT]))
    cg.add(var.set_default_current(config[CONF_DEFAULT_CURRENT]))
    cg.add(var.set_allow_ventilation(config[CONF_ALLOW_VENTILATION]))

    cg.add(var.set_sample_interval(config[CONF_SAMPLE_INTERVAL].total_milliseconds))
    cg.add(var.set_sample_window_us(config[CONF_SAMPLE_WINDOW_US]))  # legacy, ignored by DMA sampler
    cg.add(var.set_adc_sample_rate(config[CONF_ADC_SAMPLE_RATE]))
    cg.add(var.set_adc_peak_samples(config[CONF_ADC_PEAK_SAMPLES]))
    cg.add(var.set_adc_min_samples(config[CONF_ADC_MIN_SAMPLES]))
    cg.add(var.set_adc_fault_time(config[CONF_ADC_FAULT_TIME].total_milliseconds))
    cg.add(var.set_cp_confirm_windows(config[CONF_CP_CONFIRM_WINDOWS]))
    cg.add(var.set_stable_time(config[CONF_STABLE_TIME].total_milliseconds))
    cg.add(var.set_contactor_close_delay(config[CONF_CONTACTOR_CLOSE_DELAY].total_milliseconds))
    cg.add(var.set_graceful_stop_timeout(config[CONF_GRACEFUL_STOP_TIMEOUT].total_milliseconds))
    cg.add(var.set_diode_fault_time(config[CONF_DIODE_FAULT_TIME].total_milliseconds))
    cg.add(var.set_fault_retry_time(config[CONF_FAULT_RETRY_TIME].total_milliseconds))

    cg.add(var.set_task_core(config[CONF_TASK_CORE]))
    cg.add(var.set_task_priority(config[CONF_TASK_PRIORITY]))
    cg.add(var.set_task_stack_size(config[CONF_TASK_STACK_SIZE]))

    timing_sensor_keys = (
        CONF_TASK_LATE_CYCLES,
        CONF_TASK_MAX_RUNTIME,
        CONF_TASK_LAST_RUNTIME,
        CONF_TASK_LAST_LATENESS,
        CONF_TASK_MAX_LATENESS,
        CONF_TASK_MISSED_DEADLINES,
        CONF_ADC_SAMPLE_COUNT,
        CONF_ADC_READ_ERRORS,
        CONF_PILOT_DUTY,
        CONF_PILOT_MODE,
    )
    if CONF_TIMING_DEBUG in config:
        timing_debug = config[CONF_TIMING_DEBUG]
    else:
        # Backward compatibility: a v0.4.0 YAML that already contains
        # task_late_cycles/task_max_runtime keeps working unchanged.
        timing_debug = any(key in config for key in timing_sensor_keys)
    cg.add(var.set_timing_debug(timing_debug))

    cg.add(var.set_state_a_min_mv(config[CONF_STATE_A_MIN_MV]))
    cg.add(var.set_state_a_max_mv(config[CONF_STATE_A_MAX_MV]))
    cg.add(var.set_state_b_min_mv(config[CONF_STATE_B_MIN_MV]))
    cg.add(var.set_state_b_max_mv(config[CONF_STATE_B_MAX_MV]))
    cg.add(var.set_state_c_min_mv(config[CONF_STATE_C_MIN_MV]))
    cg.add(var.set_state_c_max_mv(config[CONF_STATE_C_MAX_MV]))
    cg.add(var.set_state_d_min_mv(config[CONF_STATE_D_MIN_MV]))
    cg.add(var.set_state_d_max_mv(config[CONF_STATE_D_MAX_MV]))
    cg.add(var.set_diode_min_mv(config[CONF_DIODE_MIN_MV]))
    cg.add(var.set_diode_max_mv(config[CONF_DIODE_MAX_MV]))

    if CONF_STATE in config:
        ent = await text_sensor.new_text_sensor(config[CONF_STATE])
        cg.add(var.set_state_sensor(ent))
    if CONF_PHYSICAL_STATE in config:
        ent = await text_sensor.new_text_sensor(config[CONF_PHYSICAL_STATE])
        cg.add(var.set_physical_state_sensor(ent))
    if CONF_FAULT_REASON in config:
        ent = await text_sensor.new_text_sensor(config[CONF_FAULT_REASON])
        cg.add(var.set_fault_reason_sensor(ent))
    if CONF_CP_HIGH_MV in config:
        ent = await sensor.new_sensor(config[CONF_CP_HIGH_MV])
        cg.add(var.set_cp_high_mv_sensor(ent))
    if CONF_CP_LOW_MV in config:
        ent = await sensor.new_sensor(config[CONF_CP_LOW_MV])
        cg.add(var.set_cp_low_mv_sensor(ent))
    if CONF_ADVERTISED_CURRENT in config:
        ent = await sensor.new_sensor(config[CONF_ADVERTISED_CURRENT])
        cg.add(var.set_advertised_current_sensor(ent))
    if timing_debug:
        if CONF_TASK_LATE_CYCLES in config:
            ent = await sensor.new_sensor(config[CONF_TASK_LATE_CYCLES])
            cg.add(var.set_task_late_cycles_sensor(ent))
        if CONF_TASK_MAX_RUNTIME in config:
            ent = await sensor.new_sensor(config[CONF_TASK_MAX_RUNTIME])
            cg.add(var.set_task_max_runtime_sensor(ent))
        if CONF_TASK_LAST_RUNTIME in config:
            ent = await sensor.new_sensor(config[CONF_TASK_LAST_RUNTIME])
            cg.add(var.set_task_last_runtime_sensor(ent))
        if CONF_TASK_LAST_LATENESS in config:
            ent = await sensor.new_sensor(config[CONF_TASK_LAST_LATENESS])
            cg.add(var.set_task_last_lateness_sensor(ent))
        if CONF_TASK_MAX_LATENESS in config:
            ent = await sensor.new_sensor(config[CONF_TASK_MAX_LATENESS])
            cg.add(var.set_task_max_lateness_sensor(ent))
        if CONF_TASK_MISSED_DEADLINES in config:
            ent = await sensor.new_sensor(config[CONF_TASK_MISSED_DEADLINES])
            cg.add(var.set_task_missed_deadlines_sensor(ent))
        if CONF_ADC_SAMPLE_COUNT in config:
            ent = await sensor.new_sensor(config[CONF_ADC_SAMPLE_COUNT])
            cg.add(var.set_adc_sample_count_sensor(ent))
        if CONF_ADC_READ_ERRORS in config:
            ent = await sensor.new_sensor(config[CONF_ADC_READ_ERRORS])
            cg.add(var.set_adc_read_errors_sensor(ent))
        if CONF_PILOT_DUTY in config:
            ent = await sensor.new_sensor(config[CONF_PILOT_DUTY])
            cg.add(var.set_pilot_duty_sensor(ent))
        if CONF_PILOT_MODE in config:
            ent = await text_sensor.new_text_sensor(config[CONF_PILOT_MODE])
            cg.add(var.set_pilot_mode_sensor(ent))
    if CONF_VEHICLE_CONNECTED in config:
        ent = await binary_sensor.new_binary_sensor(config[CONF_VEHICLE_CONNECTED])
        cg.add(var.set_vehicle_connected_sensor(ent))
    if CONF_CHARGING in config:
        ent = await binary_sensor.new_binary_sensor(config[CONF_CHARGING])
        cg.add(var.set_charging_sensor(ent))
    if CONF_STOPPING in config:
        ent = await binary_sensor.new_binary_sensor(config[CONF_STOPPING])
        cg.add(var.set_stopping_sensor(ent))
    if CONF_FAULT in config:
        ent = await binary_sensor.new_binary_sensor(config[CONF_FAULT])
        cg.add(var.set_fault_sensor(ent))
    if CONF_TASK_RUNNING in config:
        ent = await binary_sensor.new_binary_sensor(config[CONF_TASK_RUNNING])
        cg.add(var.set_task_running_sensor(ent))

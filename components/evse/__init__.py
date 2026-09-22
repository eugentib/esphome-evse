import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import output, switch, sensor, binary_sensor, text_sensor
from esphome.const import CONF_ID

CODEOWNERS = []
AUTO_LOAD = ["sensor", "binary_sensor", "text_sensor", "output", "switch"]

CONF_PILOT_OUTPUT = "pilot_output"
CONF_PILOT_ADC_PIN = "pilot_adc_pin"
CONF_CONTACTOR = "contactor"
CONF_MAX_CURRENT = "max_current"
CONF_DEFAULT_CURRENT = "default_current"
CONF_ALLOW_VENTILATION = "allow_ventilation"
CONF_SAMPLE_INTERVAL = "sample_interval"
CONF_SAMPLE_WINDOW_US = "sample_window_us"
CONF_STABLE_TIME = "stable_time"
CONF_CONTACTOR_CLOSE_DELAY = "contactor_close_delay"
CONF_GRACEFUL_STOP_TIMEOUT = "graceful_stop_timeout"
CONF_DIODE_FAULT_TIME = "diode_fault_time"
CONF_FAULT_RETRY_TIME = "fault_retry_time"

CONF_STATE_A_MIN_RAW = "state_a_min_raw"
CONF_STATE_A_MAX_RAW = "state_a_max_raw"
CONF_STATE_B_MIN_RAW = "state_b_min_raw"
CONF_STATE_B_MAX_RAW = "state_b_max_raw"
CONF_STATE_C_MIN_RAW = "state_c_min_raw"
CONF_STATE_C_MAX_RAW = "state_c_max_raw"
CONF_STATE_D_MIN_RAW = "state_d_min_raw"
CONF_STATE_D_MAX_RAW = "state_d_max_raw"
CONF_DIODE_MIN_RAW = "diode_min_raw"
CONF_DIODE_MAX_RAW = "diode_max_raw"

CONF_STATE = "state"
CONF_PHYSICAL_STATE = "physical_state"
CONF_FAULT_REASON = "fault_reason"
CONF_CP_HIGH_RAW = "cp_high_raw"
CONF_CP_LOW_RAW = "cp_low_raw"
CONF_ADVERTISED_CURRENT = "advertised_current"
CONF_VEHICLE_CONNECTED = "vehicle_connected"
CONF_CHARGING = "charging"
CONF_STOPPING = "stopping"
CONF_FAULT = "fault"

evse_ns = cg.esphome_ns.namespace("evse")
EVSEComponent = evse_ns.class_("EVSEComponent", cg.Component)

raw = cv.int_range(min=0, max=4095)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(EVSEComponent),
    cv.Required(CONF_PILOT_OUTPUT): cv.use_id(output.FloatOutput),
    cv.Required(CONF_PILOT_ADC_PIN): pins.internal_gpio_input_pin_schema,
    cv.Required(CONF_CONTACTOR): cv.use_id(switch.Switch),

    cv.Optional(CONF_MAX_CURRENT, default=16.0): cv.float_range(min=6.0, max=32.0),
    cv.Optional(CONF_DEFAULT_CURRENT, default=6.0): cv.float_range(min=6.0, max=32.0),
    cv.Optional(CONF_ALLOW_VENTILATION, default=False): cv.boolean,

    cv.Optional(CONF_SAMPLE_INTERVAL, default="20ms"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_SAMPLE_WINDOW_US, default=1600): cv.int_range(min=1000, max=5000),
    cv.Optional(CONF_STABLE_TIME, default="250ms"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_CONTACTOR_CLOSE_DELAY, default="1ms"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_GRACEFUL_STOP_TIMEOUT, default="6s"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_DIODE_FAULT_TIME, default="100ms"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_FAULT_RETRY_TIME, default="60s"): cv.positive_time_period_milliseconds,

    cv.Required(CONF_STATE_A_MIN_RAW): raw,
    cv.Required(CONF_STATE_A_MAX_RAW): raw,
    cv.Required(CONF_STATE_B_MIN_RAW): raw,
    cv.Required(CONF_STATE_B_MAX_RAW): raw,
    cv.Required(CONF_STATE_C_MIN_RAW): raw,
    cv.Required(CONF_STATE_C_MAX_RAW): raw,
    cv.Required(CONF_STATE_D_MIN_RAW): raw,
    cv.Required(CONF_STATE_D_MAX_RAW): raw,
    cv.Required(CONF_DIODE_MIN_RAW): raw,
    cv.Required(CONF_DIODE_MAX_RAW): raw,

    cv.Optional(CONF_STATE): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_PHYSICAL_STATE): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_FAULT_REASON): text_sensor.text_sensor_schema(),
    cv.Optional(CONF_CP_HIGH_RAW): sensor.sensor_schema(accuracy_decimals=0, icon="mdi:sine-wave"),
    cv.Optional(CONF_CP_LOW_RAW): sensor.sensor_schema(accuracy_decimals=0, icon="mdi:sine-wave"),
    cv.Optional(CONF_ADVERTISED_CURRENT): sensor.sensor_schema(unit_of_measurement="A", accuracy_decimals=1, icon="mdi:current-ac"),
    cv.Optional(CONF_VEHICLE_CONNECTED): binary_sensor.binary_sensor_schema(),
    cv.Optional(CONF_CHARGING): binary_sensor.binary_sensor_schema(),
    cv.Optional(CONF_STOPPING): binary_sensor.binary_sensor_schema(),
    cv.Optional(CONF_FAULT): binary_sensor.binary_sensor_schema(),
}).extend(cv.COMPONENT_SCHEMA)


def _validate(config):
    if config[CONF_DEFAULT_CURRENT] > config[CONF_MAX_CURRENT]:
        raise cv.Invalid("default_current cannot exceed max_current")

    windows = [
        ("D", config[CONF_STATE_D_MIN_RAW], config[CONF_STATE_D_MAX_RAW]),
        ("C", config[CONF_STATE_C_MIN_RAW], config[CONF_STATE_C_MAX_RAW]),
        ("B", config[CONF_STATE_B_MIN_RAW], config[CONF_STATE_B_MAX_RAW]),
        ("A", config[CONF_STATE_A_MIN_RAW], config[CONF_STATE_A_MAX_RAW]),
    ]
    for name, lo, hi in windows:
        if lo > hi:
            raise cv.Invalid(f"state_{name.lower()}_min_raw must be <= state_{name.lower()}_max_raw")

    # Raw ADC rises with positive CP voltage. Keep explicit invalid gaps between
    # windows instead of classifying every possible ADC value as a valid state.
    if not (
        config[CONF_STATE_D_MAX_RAW] < config[CONF_STATE_C_MIN_RAW]
        and config[CONF_STATE_C_MAX_RAW] < config[CONF_STATE_B_MIN_RAW]
        and config[CONF_STATE_B_MAX_RAW] < config[CONF_STATE_A_MIN_RAW]
    ):
        raise cv.Invalid("CP A/B/C/D windows must be ordered, non-overlapping, and leave invalid gaps")

    if config[CONF_DIODE_MIN_RAW] > config[CONF_DIODE_MAX_RAW]:
        raise cv.Invalid("diode_min_raw must be <= diode_max_raw")

    return config


CONFIG_SCHEMA = cv.All(CONFIG_SCHEMA, _validate)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pilot = await cg.get_variable(config[CONF_PILOT_OUTPUT])
    contactor = await cg.get_variable(config[CONF_CONTACTOR])
    pilot_adc_pin = await cg.gpio_pin_expression(config[CONF_PILOT_ADC_PIN])

    cg.add(var.set_pilot_output(pilot))
    cg.add(var.set_pilot_adc_pin(pilot_adc_pin))
    cg.add(var.set_contactor(contactor))
    cg.add(var.set_max_current(config[CONF_MAX_CURRENT]))
    cg.add(var.set_default_current(config[CONF_DEFAULT_CURRENT]))
    cg.add(var.set_allow_ventilation(config[CONF_ALLOW_VENTILATION]))

    cg.add(var.set_sample_interval(config[CONF_SAMPLE_INTERVAL].total_milliseconds))
    cg.add(var.set_sample_window_us(config[CONF_SAMPLE_WINDOW_US]))
    cg.add(var.set_stable_time(config[CONF_STABLE_TIME].total_milliseconds))
    cg.add(var.set_contactor_close_delay(config[CONF_CONTACTOR_CLOSE_DELAY].total_milliseconds))
    cg.add(var.set_graceful_stop_timeout(config[CONF_GRACEFUL_STOP_TIMEOUT].total_milliseconds))
    cg.add(var.set_diode_fault_time(config[CONF_DIODE_FAULT_TIME].total_milliseconds))
    cg.add(var.set_fault_retry_time(config[CONF_FAULT_RETRY_TIME].total_milliseconds))

    cg.add(var.set_state_a_min_raw(config[CONF_STATE_A_MIN_RAW]))
    cg.add(var.set_state_a_max_raw(config[CONF_STATE_A_MAX_RAW]))
    cg.add(var.set_state_b_min_raw(config[CONF_STATE_B_MIN_RAW]))
    cg.add(var.set_state_b_max_raw(config[CONF_STATE_B_MAX_RAW]))
    cg.add(var.set_state_c_min_raw(config[CONF_STATE_C_MIN_RAW]))
    cg.add(var.set_state_c_max_raw(config[CONF_STATE_C_MAX_RAW]))
    cg.add(var.set_state_d_min_raw(config[CONF_STATE_D_MIN_RAW]))
    cg.add(var.set_state_d_max_raw(config[CONF_STATE_D_MAX_RAW]))
    cg.add(var.set_diode_min_raw(config[CONF_DIODE_MIN_RAW]))
    cg.add(var.set_diode_max_raw(config[CONF_DIODE_MAX_RAW]))

    if CONF_STATE in config:
        ent = await text_sensor.new_text_sensor(config[CONF_STATE]); cg.add(var.set_state_sensor(ent))
    if CONF_PHYSICAL_STATE in config:
        ent = await text_sensor.new_text_sensor(config[CONF_PHYSICAL_STATE]); cg.add(var.set_physical_state_sensor(ent))
    if CONF_FAULT_REASON in config:
        ent = await text_sensor.new_text_sensor(config[CONF_FAULT_REASON]); cg.add(var.set_fault_reason_sensor(ent))
    if CONF_CP_HIGH_RAW in config:
        ent = await sensor.new_sensor(config[CONF_CP_HIGH_RAW]); cg.add(var.set_cp_high_raw_sensor(ent))
    if CONF_CP_LOW_RAW in config:
        ent = await sensor.new_sensor(config[CONF_CP_LOW_RAW]); cg.add(var.set_cp_low_raw_sensor(ent))
    if CONF_ADVERTISED_CURRENT in config:
        ent = await sensor.new_sensor(config[CONF_ADVERTISED_CURRENT]); cg.add(var.set_advertised_current_sensor(ent))
    if CONF_VEHICLE_CONNECTED in config:
        ent = await binary_sensor.new_binary_sensor(config[CONF_VEHICLE_CONNECTED]); cg.add(var.set_vehicle_connected_sensor(ent))
    if CONF_CHARGING in config:
        ent = await binary_sensor.new_binary_sensor(config[CONF_CHARGING]); cg.add(var.set_charging_sensor(ent))
    if CONF_STOPPING in config:
        ent = await binary_sensor.new_binary_sensor(config[CONF_STOPPING]); cg.add(var.set_stopping_sensor(ent))
    if CONF_FAULT in config:
        ent = await binary_sensor.new_binary_sensor(config[CONF_FAULT]); cg.add(var.set_fault_sensor(ent))

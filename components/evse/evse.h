#pragma once

#include <inttypes.h>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/log.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/portmacro.h>
#endif

namespace esphome {
namespace evse {

enum class CpLevel : uint8_t { UNKNOWN = 0, A, B, C, D, INVALID };
enum class EvseState : uint8_t { A = 0, B1, B2, C1, C2, D1, D2, E, F };
enum class PilotMode : uint8_t { POSITIVE_DC = 0, PWM, NEGATIVE_DC };
enum class FaultCode : uint8_t {
  NONE = 0,
  PILOT_VOLTAGE,
  PILOT_TRANSITION,
  DIODE_FAULT,
};

class EVSEComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;

  void set_pilot_output(output::FloatOutput *out) { pilot_output_ = out; }
  void set_pilot_adc_pin(InternalGPIOPin *pin) { pilot_adc_pin_ = pin; }
  void set_contactor_pin(InternalGPIOPin *pin) { contactor_pin_ = pin; }

  void set_max_current(float amps) { max_current_ = amps; }
  void set_default_current(float amps) {
    default_current_ = amps;
    requested_current_limit_ = amps;
    current_limit_ = amps;
  }
  void set_allow_ventilation(bool value) { allow_ventilation_ = value; }

  void set_sample_interval(uint32_t ms) { sample_interval_ms_ = ms; }
  void set_sample_window_us(uint32_t us) { sample_window_us_ = us; }
  void set_stable_time(uint32_t ms) { stable_time_ms_ = ms; }
  void set_contactor_close_delay(uint32_t ms) { contactor_close_delay_ms_ = ms; }
  void set_graceful_stop_timeout(uint32_t ms) { graceful_stop_timeout_ms_ = ms; }
  void set_diode_fault_time(uint32_t ms) { diode_fault_time_ms_ = ms; }
  void set_fault_retry_time(uint32_t ms) { fault_retry_time_ms_ = ms; }

  void set_task_core(uint8_t core) { task_core_ = core; }
  void set_task_priority(uint8_t priority) { task_priority_ = priority; }
  void set_task_stack_size(uint32_t bytes) { task_stack_size_ = bytes; }

  void set_state_a_min_mv(uint16_t v) { state_a_min_mv_ = v; }
  void set_state_a_max_mv(uint16_t v) { state_a_max_mv_ = v; }
  void set_state_b_min_mv(uint16_t v) { state_b_min_mv_ = v; }
  void set_state_b_max_mv(uint16_t v) { state_b_max_mv_ = v; }
  void set_state_c_min_mv(uint16_t v) { state_c_min_mv_ = v; }
  void set_state_c_max_mv(uint16_t v) { state_c_max_mv_ = v; }
  void set_state_d_min_mv(uint16_t v) { state_d_min_mv_ = v; }
  void set_state_d_max_mv(uint16_t v) { state_d_max_mv_ = v; }
  void set_diode_min_mv(uint16_t v) { diode_min_mv_ = v; }
  void set_diode_max_mv(uint16_t v) { diode_max_mv_ = v; }

  void set_state_sensor(text_sensor::TextSensor *s) { state_sensor_ = s; }
  void set_physical_state_sensor(text_sensor::TextSensor *s) { physical_state_sensor_ = s; }
  void set_fault_reason_sensor(text_sensor::TextSensor *s) { fault_reason_sensor_ = s; }
  void set_cp_high_mv_sensor(sensor::Sensor *s) { cp_high_mv_sensor_ = s; }
  void set_cp_low_mv_sensor(sensor::Sensor *s) { cp_low_mv_sensor_ = s; }
  void set_advertised_current_sensor(sensor::Sensor *s) { advertised_current_sensor_ = s; }
  void set_task_late_cycles_sensor(sensor::Sensor *s) { task_late_cycles_sensor_ = s; }
  void set_task_max_runtime_sensor(sensor::Sensor *s) { task_max_runtime_sensor_ = s; }
  void set_vehicle_connected_sensor(binary_sensor::BinarySensor *s) { vehicle_connected_sensor_ = s; }
  void set_charging_sensor(binary_sensor::BinarySensor *s) { charging_sensor_ = s; }
  void set_stopping_sensor(binary_sensor::BinarySensor *s) { stopping_sensor_ = s; }
  void set_fault_sensor(binary_sensor::BinarySensor *s) { fault_sensor_ = s; }
  void set_task_running_sensor(binary_sensor::BinarySensor *s) { task_running_sensor_ = s; }

  void set_enabled(bool enabled);
  bool is_enabled();
  void set_available(bool available);
  bool is_available();
  void reset_fault();

  void set_current_limit(float amps);
  float get_current_limit();

 protected:
#ifdef USE_ESP32
  static void task_entry_(void *arg);
  void task_loop_();
#endif

  void process_requests_();
  void sample_cp_();
  CpLevel classify_cp_(uint16_t high_mv) const;
  void update_stable_cp_(CpLevel sampled, uint32_t now);
  void update_diode_supervision_(uint32_t now);
  void control_(uint32_t now);
  EvseState target_state_for_cp_(CpLevel cp, bool charging_allowed);
  void enter_state_(EvseState next, uint32_t now);
  void service_state_actions_(uint32_t now);

  void set_pilot_mode_(PilotMode mode);
  void apply_pilot_output_();
  void open_contactor_();
  void close_contactor_();

  void raise_fault_(FaultCode code);
  void clear_fault_(uint32_t now);

  float duty_for_current_(float amps) const;
  bool mv_in_window_(uint16_t v, uint16_t lo, uint16_t hi) const;
  bool is_energizing_state_(EvseState s) const;
  bool is_paused_active_state_(EvseState s) const;
  const char *cp_level_to_string_(CpLevel s) const;
  const char *state_to_string_(EvseState s) const;
  const char *fault_to_string_(FaultCode code) const;

  void update_snapshot_(uint32_t heartbeat_ms);
  void publish_();

  output::FloatOutput *pilot_output_{nullptr};
  InternalGPIOPin *pilot_adc_pin_{nullptr};
  InternalGPIOPin *contactor_pin_{nullptr};
  uint8_t pilot_adc_gpio_num_{0};

  text_sensor::TextSensor *state_sensor_{nullptr};
  text_sensor::TextSensor *physical_state_sensor_{nullptr};
  text_sensor::TextSensor *fault_reason_sensor_{nullptr};
  sensor::Sensor *cp_high_mv_sensor_{nullptr};
  sensor::Sensor *cp_low_mv_sensor_{nullptr};
  sensor::Sensor *advertised_current_sensor_{nullptr};
  sensor::Sensor *task_late_cycles_sensor_{nullptr};
  sensor::Sensor *task_max_runtime_sensor_{nullptr};
  binary_sensor::BinarySensor *vehicle_connected_sensor_{nullptr};
  binary_sensor::BinarySensor *charging_sensor_{nullptr};
  binary_sensor::BinarySensor *stopping_sensor_{nullptr};
  binary_sensor::BinarySensor *fault_sensor_{nullptr};
  binary_sensor::BinarySensor *task_running_sensor_{nullptr};

  // Static configuration; set before setup(), then read only.
  float max_current_{16.0f};
  float default_current_{6.0f};
  bool allow_ventilation_{false};

  uint32_t sample_interval_ms_{20};
  uint32_t sample_window_us_{3000};
  uint32_t stable_time_ms_{250};
  uint32_t contactor_close_delay_ms_{1};
  uint32_t graceful_stop_timeout_ms_{6000};
  uint32_t diode_fault_time_ms_{100};
  uint32_t fault_retry_time_ms_{60000};

  uint8_t task_core_{1};
  uint8_t task_priority_{5};
  uint32_t task_stack_size_{4096};

  uint16_t state_a_min_mv_{2550}, state_a_max_mv_{2745};
  uint16_t state_b_min_mv_{2260}, state_b_max_mv_{2455};
  uint16_t state_c_min_mv_{1970}, state_c_max_mv_{2170};
  uint16_t state_d_min_mv_{1680}, state_d_max_mv_{1880};
  uint16_t diode_min_mv_{240}, diode_max_mv_{435};

#ifdef USE_ESP32
  TaskHandle_t task_handle_{nullptr};
  mutable portMUX_TYPE data_mux_ = portMUX_INITIALIZER_UNLOCKED;
#endif

  // Requests written by the ESPHome/main-loop side, consumed by the EVSE task.
  bool requested_enabled_{false};
  bool requested_available_{true};
  bool reset_fault_requested_{false};
  float requested_current_limit_{6.0f};

  // Task-owned runtime state. Only the dedicated EVSE task mutates these after setup().
  float current_limit_{6.0f};
  bool enabled_{false};
  bool available_{true};
  bool contactor_on_{false};
  bool graceful_stop_active_{false};
  bool diode_sample_valid_{true};
  bool diode_valid_{true};

  uint16_t cp_high_mv_{0};
  uint16_t cp_low_mv_{3300};

  CpLevel sampled_cp_{CpLevel::UNKNOWN};
  CpLevel candidate_cp_{CpLevel::UNKNOWN};
  CpLevel stable_cp_{CpLevel::UNKNOWN};
  EvseState state_{EvseState::A};
  PilotMode pilot_mode_{PilotMode::POSITIVE_DC};
  FaultCode fault_code_{FaultCode::NONE};

  uint32_t candidate_since_ms_{0};
  uint32_t state_entered_ms_{0};
  uint32_t graceful_stop_started_ms_{0};
  uint32_t diode_invalid_since_ms_{0};
  uint32_t fault_since_ms_{0};
  uint32_t task_late_cycles_{0};
  uint32_t task_max_runtime_us_{0};

  // Coherent status snapshot consumed by ESPHome loop()/Home Assistant publishing.
  EvseState snapshot_state_{EvseState::A};
  CpLevel snapshot_cp_{CpLevel::UNKNOWN};
  FaultCode snapshot_fault_{FaultCode::NONE};
  uint16_t snapshot_cp_high_mv_{0};
  uint16_t snapshot_cp_low_mv_{3300};
  float snapshot_advertised_current_{0.0f};
  bool snapshot_contactor_on_{false};
  bool snapshot_graceful_stop_{false};
  uint32_t snapshot_task_heartbeat_ms_{0};
  uint32_t snapshot_task_late_cycles_{0};
  uint32_t snapshot_task_max_runtime_us_{0};

  uint32_t last_publish_ms_{0};
};

}  // namespace evse
}  // namespace esphome

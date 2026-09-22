#pragma once

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/log.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace evse {

enum class CpLevel : uint8_t { UNKNOWN = 0, A, B, C, D, INVALID };
enum class EvseState : uint8_t { A = 0, B1, B2, C1, C2, D1, D2, E, F };
enum class PilotMode : uint8_t { POSITIVE_DC = 0, PWM, NEGATIVE_DC };
enum class FaultCode : uint8_t { NONE = 0, PILOT_FAULT, DIODE_FAULT };

class EVSEComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_pilot_output(output::FloatOutput *out) { pilot_output_ = out; }
  void set_pilot_adc_pin(InternalGPIOPin *pin) { pilot_adc_pin_ = pin; }
  void set_contactor(switch_::Switch *sw) { contactor_ = sw; }

  void set_max_current(float amps) { max_current_ = amps; }
  void set_default_current(float amps) { default_current_ = current_limit_ = amps; }
  void set_allow_ventilation(bool value) { allow_ventilation_ = value; }

  void set_sample_interval(uint32_t ms) { sample_interval_ms_ = ms; }
  void set_sample_window_us(uint32_t us) { sample_window_us_ = us; }
  void set_stable_time(uint32_t ms) { stable_time_ms_ = ms; }
  void set_contactor_close_delay(uint32_t ms) { contactor_close_delay_ms_ = ms; }
  void set_graceful_stop_timeout(uint32_t ms) { graceful_stop_timeout_ms_ = ms; }
  void set_diode_fault_time(uint32_t ms) { diode_fault_time_ms_ = ms; }
  void set_fault_retry_time(uint32_t ms) { fault_retry_time_ms_ = ms; }

  void set_state_a_min_raw(uint16_t v) { state_a_min_raw_ = v; }
  void set_state_a_max_raw(uint16_t v) { state_a_max_raw_ = v; }
  void set_state_b_min_raw(uint16_t v) { state_b_min_raw_ = v; }
  void set_state_b_max_raw(uint16_t v) { state_b_max_raw_ = v; }
  void set_state_c_min_raw(uint16_t v) { state_c_min_raw_ = v; }
  void set_state_c_max_raw(uint16_t v) { state_c_max_raw_ = v; }
  void set_state_d_min_raw(uint16_t v) { state_d_min_raw_ = v; }
  void set_state_d_max_raw(uint16_t v) { state_d_max_raw_ = v; }
  void set_diode_min_raw(uint16_t v) { diode_min_raw_ = v; }
  void set_diode_max_raw(uint16_t v) { diode_max_raw_ = v; }

  void set_state_sensor(text_sensor::TextSensor *s) { state_sensor_ = s; }
  void set_physical_state_sensor(text_sensor::TextSensor *s) { physical_state_sensor_ = s; }
  void set_fault_reason_sensor(text_sensor::TextSensor *s) { fault_reason_sensor_ = s; }
  void set_cp_high_raw_sensor(sensor::Sensor *s) { cp_high_raw_sensor_ = s; }
  void set_cp_low_raw_sensor(sensor::Sensor *s) { cp_low_raw_sensor_ = s; }
  void set_advertised_current_sensor(sensor::Sensor *s) { advertised_current_sensor_ = s; }
  void set_vehicle_connected_sensor(binary_sensor::BinarySensor *s) { vehicle_connected_sensor_ = s; }
  void set_charging_sensor(binary_sensor::BinarySensor *s) { charging_sensor_ = s; }
  void set_stopping_sensor(binary_sensor::BinarySensor *s) { stopping_sensor_ = s; }
  void set_fault_sensor(binary_sensor::BinarySensor *s) { fault_sensor_ = s; }

  void set_enabled(bool enabled);
  bool is_enabled() const { return enabled_; }
  void set_available(bool available);
  bool is_available() const { return available_; }
  void reset_fault();

  void set_current_limit(float amps);
  float get_current_limit() const { return current_limit_; }

 protected:
  void sample_cp_();
  CpLevel classify_cp_(uint16_t high_raw) const;
  void update_stable_cp_(CpLevel sampled);
  void update_diode_supervision_(uint32_t now);
  void control_();
  EvseState target_state_for_cp_(CpLevel cp, bool charging_allowed);
  void enter_state_(EvseState next);
  void service_state_actions_(uint32_t now);

  void set_pilot_mode_(PilotMode mode);
  void apply_pilot_output_();
  void open_contactor_();
  void close_contactor_();

  void raise_fault_(FaultCode code, const char *reason);
  void clear_fault_();

  float duty_for_current_(float amps) const;
  bool raw_in_window_(uint16_t v, uint16_t lo, uint16_t hi) const;
  bool is_energizing_state_(EvseState s) const;
  bool is_paused_active_state_(EvseState s) const;
  const char *cp_level_to_string_(CpLevel s) const;
  const char *state_to_string_(EvseState s) const;
  const char *fault_to_string_() const;
  void publish_();

  output::FloatOutput *pilot_output_{nullptr};
  InternalGPIOPin *pilot_adc_pin_{nullptr};
  uint8_t pilot_adc_gpio_num_{0};
  switch_::Switch *contactor_{nullptr};

  text_sensor::TextSensor *state_sensor_{nullptr};
  text_sensor::TextSensor *physical_state_sensor_{nullptr};
  text_sensor::TextSensor *fault_reason_sensor_{nullptr};
  sensor::Sensor *cp_high_raw_sensor_{nullptr};
  sensor::Sensor *cp_low_raw_sensor_{nullptr};
  sensor::Sensor *advertised_current_sensor_{nullptr};
  binary_sensor::BinarySensor *vehicle_connected_sensor_{nullptr};
  binary_sensor::BinarySensor *charging_sensor_{nullptr};
  binary_sensor::BinarySensor *stopping_sensor_{nullptr};
  binary_sensor::BinarySensor *fault_sensor_{nullptr};

  float max_current_{16.0f};
  float default_current_{6.0f};
  float current_limit_{6.0f};
  bool allow_ventilation_{false};

  uint32_t sample_interval_ms_{20};
  uint32_t sample_window_us_{1600};
  uint32_t stable_time_ms_{250};
  uint32_t contactor_close_delay_ms_{1};
  uint32_t graceful_stop_timeout_ms_{6000};
  uint32_t diode_fault_time_ms_{100};
  uint32_t fault_retry_time_ms_{60000};

  uint16_t state_a_min_raw_{3550}, state_a_max_raw_{3950};
  uint16_t state_b_min_raw_{3150}, state_b_max_raw_{3475};
  uint16_t state_c_min_raw_{2725}, state_c_max_raw_{3075};
  uint16_t state_d_min_raw_{2300}, state_d_max_raw_{2650};
  uint16_t diode_min_raw_{200}, diode_max_raw_{600};

  uint16_t cp_high_raw_{0};
  uint16_t cp_low_raw_{4095};

  CpLevel sampled_cp_{CpLevel::UNKNOWN};
  CpLevel candidate_cp_{CpLevel::UNKNOWN};
  CpLevel stable_cp_{CpLevel::UNKNOWN};
  EvseState state_{EvseState::A};
  PilotMode pilot_mode_{PilotMode::POSITIVE_DC};
  FaultCode fault_code_{FaultCode::NONE};

  uint32_t candidate_since_ms_{0};
  uint32_t state_entered_ms_{0};
  uint32_t last_sample_ms_{0};
  uint32_t last_publish_ms_{0};
  uint32_t graceful_stop_started_ms_{0};
  uint32_t diode_invalid_since_ms_{0};
  uint32_t fault_since_ms_{0};

  bool enabled_{false};
  bool available_{true};
  bool contactor_on_{false};
  bool graceful_stop_active_{false};
  bool diode_sample_valid_{true};
  bool diode_valid_{true};
  std::string fault_reason_{"None"};
};

}  // namespace evse
}  // namespace esphome

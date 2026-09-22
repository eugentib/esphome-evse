#pragma once
#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/log.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
namespace esphome { namespace evse {
enum class PilotState : uint8_t { UNKNOWN=0, A, B, C, D, E };
class EVSEComponent : public Component {
 public:
  void setup() override; void loop() override; void dump_config() override;
  void set_pilot_output(output::FloatOutput *v){pilot_output_=v;}
  void set_pilot_adc_pin(InternalGPIOPin *v){pilot_adc_pin_=v;}
  void set_contactor(switch_::Switch *v){contactor_=v;}
  void set_max_current(float v){max_current_=v;}
  void set_default_current(float v){default_current_=current_limit_=v;}
  void set_sample_interval(uint32_t v){sample_interval_ms_=v;}
  void set_sample_window_us(uint32_t v){sample_window_us_=v;}
  void set_stable_time(uint32_t v){stable_time_ms_=v;}
  void set_contactor_close_delay(uint32_t v){contactor_close_delay_ms_=v;}
  void set_state_a_min_raw(uint16_t v){state_a_min_raw_=v;}
  void set_state_b_min_raw(uint16_t v){state_b_min_raw_=v;}
  void set_state_c_min_raw(uint16_t v){state_c_min_raw_=v;}
  void set_state_d_min_raw(uint16_t v){state_d_min_raw_=v;}
  void set_diode_max_raw(uint16_t v){diode_max_raw_=v;}
  void set_state_sensor(text_sensor::TextSensor *v){state_sensor_=v;}
  void set_cp_high_raw_sensor(sensor::Sensor *v){cp_high_raw_sensor_=v;}
  void set_cp_low_raw_sensor(sensor::Sensor *v){cp_low_raw_sensor_=v;}
  void set_advertised_current_sensor(sensor::Sensor *v){advertised_current_sensor_=v;}
  void set_vehicle_connected_sensor(binary_sensor::BinarySensor *v){vehicle_connected_sensor_=v;}
  void set_charging_sensor(binary_sensor::BinarySensor *v){charging_sensor_=v;}
  void set_fault_sensor(binary_sensor::BinarySensor *v){fault_sensor_=v;}
  void set_enabled(bool v); bool is_enabled() const{return enabled_;}
  void set_current_limit(float v); float get_current_limit() const{return current_limit_;}
 protected:
  void sample_cp_(); PilotState classify_state_(uint16_t) const; void update_stable_state_(PilotState); void control_();
  void set_pwm_active_(bool); void apply_pilot_output_(); void open_contactor_(); void close_contactor_();
  void set_fault_(bool,const char *reason=nullptr); void publish_(); float duty_for_current_(float) const;
  const char *physical_state_to_string_(PilotState) const; const char *reported_state_to_string_() const;
  output::FloatOutput *pilot_output_{nullptr}; InternalGPIOPin *pilot_adc_pin_{nullptr}; uint8_t pilot_adc_gpio_num_{0}; switch_::Switch *contactor_{nullptr};
  text_sensor::TextSensor *state_sensor_{nullptr}; sensor::Sensor *cp_high_raw_sensor_{nullptr}; sensor::Sensor *cp_low_raw_sensor_{nullptr}; sensor::Sensor *advertised_current_sensor_{nullptr};
  binary_sensor::BinarySensor *vehicle_connected_sensor_{nullptr}; binary_sensor::BinarySensor *charging_sensor_{nullptr}; binary_sensor::BinarySensor *fault_sensor_{nullptr};
  float max_current_{16.0f},default_current_{6.0f},current_limit_{6.0f};
  uint32_t sample_interval_ms_{20},sample_window_us_{1600},stable_time_ms_{250},contactor_close_delay_ms_{500};
  uint16_t state_a_min_raw_{3500},state_b_min_raw_{2800},state_c_min_raw_{2000},state_d_min_raw_{1200},diode_max_raw_{700};
  uint16_t cp_high_raw_{0},cp_low_raw_{4095};
  PilotState sampled_state_{PilotState::UNKNOWN},candidate_state_{PilotState::UNKNOWN},stable_state_{PilotState::UNKNOWN};
  uint32_t candidate_since_ms_{0},stable_since_ms_{0},last_sample_ms_{0},last_publish_ms_{0};
  bool enabled_{false},pwm_active_{false},contactor_on_{false},fault_{false},diode_ok_{false}; std::string fault_reason_;
};
}} // namespace esphome::evse

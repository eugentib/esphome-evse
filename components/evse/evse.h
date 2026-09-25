#pragma once

#include <inttypes.h>

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "esphome/core/log.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#ifdef USE_ESP32
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/portmacro.h>
#include <esp_adc/adc_continuous.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <soc/soc_caps.h>
#include <esp_err.h>
#include <esp_timer.h>
#include <driver/ledc.h>
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
  ADC_FAULT,
  PILOT_OUTPUT,
};

class EVSEComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void on_shutdown() override;

  void set_pilot_pwm_pin(InternalGPIOPin *pin) { pilot_pwm_pin_ = pin; }
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
  void set_sample_window_us(uint32_t us) { sample_window_us_ = us; }  // legacy v0.4.1
  void set_adc_sample_rate(uint32_t hz) { adc_sample_rate_hz_ = hz; }
  void set_adc_peak_samples(uint8_t count) { adc_peak_samples_ = count; }
  void set_adc_min_samples(uint16_t count) { adc_min_samples_ = count; }
  void set_adc_fault_time(uint32_t ms) { adc_fault_time_ms_ = ms; }
  void set_cp_confirm_windows(uint8_t count) { cp_confirm_windows_ = count; }
  void set_stable_time(uint32_t ms) { stable_time_ms_ = ms; }
  void set_invalid_grace_time(uint32_t ms) { invalid_grace_time_ms_ = ms; }
  void set_contactor_close_delay(uint32_t ms) { contactor_close_delay_ms_ = ms; }
  void set_graceful_stop_timeout(uint32_t ms) { graceful_stop_timeout_ms_ = ms; }
  void set_diode_fault_time(uint32_t ms) { diode_fault_time_ms_ = ms; }
  void set_fault_retry_time(uint32_t ms) { fault_retry_time_ms_ = ms; }

  void set_task_core(uint8_t core) { task_core_ = core; }
  void set_task_priority(uint8_t priority) { task_priority_ = priority; }
  void set_task_stack_size(uint32_t bytes) { task_stack_size_ = bytes; }
  void set_timing_debug(bool value) { timing_debug_ = value; }

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
  void set_task_last_runtime_sensor(sensor::Sensor *s) { task_last_runtime_sensor_ = s; }
  void set_task_last_lateness_sensor(sensor::Sensor *s) { task_last_lateness_sensor_ = s; }
  void set_task_max_lateness_sensor(sensor::Sensor *s) { task_max_lateness_sensor_ = s; }
  void set_task_missed_deadlines_sensor(sensor::Sensor *s) { task_missed_deadlines_sensor_ = s; }
  void set_adc_sample_count_sensor(sensor::Sensor *s) { adc_sample_count_sensor_ = s; }
  void set_adc_read_errors_sensor(sensor::Sensor *s) { adc_read_errors_sensor_ = s; }
  void set_pilot_duty_sensor(sensor::Sensor *s) { pilot_duty_sensor_ = s; }
  void set_pilot_mode_sensor(text_sensor::TextSensor *s) { pilot_mode_sensor_ = s; }
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
#ifdef USE_ESP32
  bool setup_pilot_pwm_();
  void shutdown_pilot_pwm_();
  bool start_pilot_pwm_(float duty_fraction);
  bool set_pilot_static_(bool high);
  bool setup_adc_dma_();
  void shutdown_adc_dma_();
  void flush_adc_dma_();
#endif
  void sample_cp_();
  void update_adc_supervision_(uint32_t now);
  CpLevel classify_cp_(uint16_t high_mv) const;
  void update_stable_cp_(CpLevel sampled, uint32_t now);
  void update_diode_supervision_(uint32_t now);
  void control_(uint32_t now);
  EvseState target_state_for_cp_(CpLevel cp, bool charging_allowed);
  void enter_state_(EvseState next, uint32_t now);
  void service_state_actions_(uint32_t now);

  void set_pilot_mode_(PilotMode mode);
  bool apply_pilot_output_();
  void open_contactor_();
  void close_contactor_();

  void raise_fault_(FaultCode code);
  void clear_fault_(uint32_t now);

  static uint32_t now_ms_();
  static uint32_t now_us32_();
  float duty_for_current_(float amps) const;
  bool mv_in_window_(uint16_t v, uint16_t lo, uint16_t hi) const;
  bool is_energizing_state_(EvseState s) const;
  bool is_paused_active_state_(EvseState s) const;
  const char *cp_level_to_string_(CpLevel s) const;
  const char *state_to_string_(EvseState s) const;
  const char *fault_to_string_(FaultCode code) const;

  void update_snapshot_(uint32_t heartbeat_ms);
  void publish_();

  InternalGPIOPin *pilot_pwm_pin_{nullptr};
  InternalGPIOPin *pilot_adc_pin_{nullptr};
  InternalGPIOPin *contactor_pin_{nullptr};
  uint8_t pilot_pwm_gpio_num_{0};
  uint8_t pilot_adc_gpio_num_{0};

  text_sensor::TextSensor *state_sensor_{nullptr};
  text_sensor::TextSensor *physical_state_sensor_{nullptr};
  text_sensor::TextSensor *fault_reason_sensor_{nullptr};
  sensor::Sensor *cp_high_mv_sensor_{nullptr};
  sensor::Sensor *cp_low_mv_sensor_{nullptr};
  sensor::Sensor *advertised_current_sensor_{nullptr};
  sensor::Sensor *task_late_cycles_sensor_{nullptr};
  sensor::Sensor *task_max_runtime_sensor_{nullptr};
  sensor::Sensor *task_last_runtime_sensor_{nullptr};
  sensor::Sensor *task_last_lateness_sensor_{nullptr};
  sensor::Sensor *task_max_lateness_sensor_{nullptr};
  sensor::Sensor *task_missed_deadlines_sensor_{nullptr};
  sensor::Sensor *adc_sample_count_sensor_{nullptr};
  sensor::Sensor *adc_read_errors_sensor_{nullptr};
  sensor::Sensor *pilot_duty_sensor_{nullptr};
  text_sensor::TextSensor *pilot_mode_sensor_{nullptr};
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
  uint32_t sample_window_us_{3000};  // legacy v0.4.1; DMA sampler ignores it
  uint32_t adc_sample_rate_hz_{80000};
  uint8_t adc_peak_samples_{16};
  uint16_t adc_min_samples_{200};
  uint32_t adc_fault_time_ms_{100};
  uint8_t cp_confirm_windows_{3};
  uint32_t stable_time_ms_{250};
  uint32_t invalid_grace_time_ms_{100};
  uint32_t contactor_close_delay_ms_{1};
  uint32_t graceful_stop_timeout_ms_{6000};
  uint32_t diode_fault_time_ms_{100};
  uint32_t fault_retry_time_ms_{60000};

  uint8_t task_core_{1};
  uint8_t task_priority_{5};
  uint32_t task_stack_size_{4096};
  bool timing_debug_{false};

  uint16_t state_a_min_mv_{2480}, state_a_max_mv_{2700};
  uint16_t state_b_min_mv_{2210}, state_b_max_mv_{2430};
  uint16_t state_c_min_mv_{1935}, state_c_max_mv_{2160};
  uint16_t state_d_min_mv_{1660}, state_d_max_mv_{1885};
  uint16_t diode_min_mv_{300}, diode_max_mv_{520};

#ifdef USE_ESP32
  TaskHandle_t task_handle_{nullptr};
  mutable portMUX_TYPE data_mux_ = portMUX_INITIALIZER_UNLOCKED;
  adc_continuous_handle_t adc_dma_handle_{nullptr};
  adc_cali_handle_t adc_cali_handle_{nullptr};
  adc_channel_t adc_channel_{ADC_CHANNEL_0};
  alignas(4) uint8_t adc_read_buffer_[1024]{};

  static constexpr ledc_mode_t PILOT_LEDC_MODE = LEDC_HIGH_SPEED_MODE;
  static constexpr ledc_timer_t PILOT_LEDC_TIMER = LEDC_TIMER_0;
  static constexpr ledc_channel_t PILOT_LEDC_CHANNEL = LEDC_CHANNEL_0;
  static constexpr ledc_timer_bit_t PILOT_LEDC_RESOLUTION = LEDC_TIMER_10_BIT;
  static constexpr uint32_t PILOT_LEDC_COUNTS = 1U << 10;
  static constexpr uint32_t PILOT_PWM_HZ = 1000;
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
  bool adc_sample_valid_{false};
  bool pilot_hw_ok_{false};
  bool pilot_pwm_running_{false};
  float pilot_duty_percent_{100.0f};

  uint16_t cp_high_mv_{0};
  uint16_t cp_low_mv_{3300};

  CpLevel sampled_cp_{CpLevel::UNKNOWN};
  CpLevel candidate_cp_{CpLevel::UNKNOWN};
  CpLevel stable_cp_{CpLevel::UNKNOWN};
  uint8_t candidate_windows_{0};
  EvseState state_{EvseState::A};
  PilotMode pilot_mode_{PilotMode::POSITIVE_DC};
  FaultCode fault_code_{FaultCode::NONE};

  uint32_t candidate_since_ms_{0};
  uint32_t invalid_since_ms_{0};
  uint32_t state_entered_ms_{0};
  uint32_t graceful_stop_started_ms_{0};
  uint32_t diode_invalid_since_ms_{0};
  uint32_t fault_since_ms_{0};
  uint32_t adc_invalid_since_ms_{0};
  uint32_t adc_last_sample_count_{0};
  uint32_t adc_read_errors_{0};
  uint32_t task_late_cycles_{0};
  uint32_t task_last_runtime_us_{0};
  uint32_t task_max_runtime_us_{0};
  uint32_t task_last_lateness_us_{0};
  uint32_t task_max_lateness_us_{0};
  uint32_t task_missed_deadlines_{0};

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
  uint32_t snapshot_task_last_runtime_us_{0};
  uint32_t snapshot_task_max_runtime_us_{0};
  uint32_t snapshot_task_last_lateness_us_{0};
  uint32_t snapshot_task_max_lateness_us_{0};
  uint32_t snapshot_task_missed_deadlines_{0};
  uint32_t snapshot_adc_sample_count_{0};
  uint32_t snapshot_adc_read_errors_{0};
  float snapshot_pilot_duty_percent_{100.0f};
  PilotMode snapshot_pilot_mode_{PilotMode::POSITIVE_DC};

  uint32_t last_publish_ms_{0};
};

}  // namespace evse
}  // namespace esphome

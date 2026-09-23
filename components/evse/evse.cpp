#include "evse.h"

#ifdef USE_ARDUINO
#include <Arduino.h>
#endif

namespace esphome {
namespace evse {

static const char *const TAG = "evse";

void EVSEComponent::setup() {
#ifndef USE_ESP32
  ESP_LOGE(TAG, "v0.4.1 requires ESP32");
  mark_failed();
  return;
#else
#ifndef USE_ARDUINO
  ESP_LOGE(TAG, "v0.4.1 currently requires the Arduino framework");
  mark_failed();
  return;
#else
  if (pilot_adc_pin_ == nullptr || contactor_pin_ == nullptr || pilot_output_ == nullptr) {
    ESP_LOGE(TAG, "Pilot ADC, contactor pin and pilot output must all be configured");
    mark_failed();
    return;
  }

  pilot_adc_pin_->setup();
  pilot_adc_gpio_num_ = pilot_adc_pin_->get_pin();
  analogReadResolution(12);
  analogSetPinAttenuation(pilot_adc_gpio_num_, ADC_11db);

  contactor_pin_->setup();
  contactor_pin_->digital_write(false);
  contactor_on_ = false;

  enabled_ = false;
  available_ = true;
  current_limit_ = default_current_;
  state_ = EvseState::A;
  fault_code_ = FaultCode::NONE;
  graceful_stop_active_ = false;
  pilot_mode_ = PilotMode::POSITIVE_DC;
  apply_pilot_output_();

  const uint32_t now = millis();
  candidate_since_ms_ = now;
  state_entered_ms_ = now;
  update_snapshot_(now);

  const BaseType_t rc = xTaskCreatePinnedToCore(
      &EVSEComponent::task_entry_,
      "evse_ctrl",
      task_stack_size_,
      this,
      task_priority_,
      &task_handle_,
      task_core_
  );

  if (rc != pdPASS) {
    ESP_LOGE(TAG, "Failed to create EVSE FreeRTOS task");
    contactor_pin_->digital_write(false);
    mark_failed();
    return;
  }

  ESP_LOGI(
      TAG,
      "EVSE v0.4.1 initialized; ADC=GPIO%u, task core=%u priority=%u stack=%" PRIu32 " B",
      pilot_adc_gpio_num_,
      task_core_,
      task_priority_,
      task_stack_size_
  );
#endif
#endif
}

void EVSEComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ESPHome EVSE v0.4.1:");
  LOG_PIN("  Pilot ADC Pin: ", pilot_adc_pin_);
  LOG_PIN("  Contactor Pin: ", contactor_pin_);
  ESP_LOGCONFIG(TAG, "  Max/default current: %.1f / %.1f A", max_current_, default_current_);
  ESP_LOGCONFIG(TAG, "  Allow State D charging: %s", YESNO(allow_ventilation_));
  ESP_LOGCONFIG(TAG, "  EVSE task: core=%u priority=%u stack=%" PRIu32 " B", task_core_, task_priority_, task_stack_size_);
  ESP_LOGCONFIG(TAG, "  Timing debug: %s", YESNO(timing_debug_));
  ESP_LOGCONFIG(TAG, "  Task/sample interval: %" PRIu32 " ms", sample_interval_ms_);
  ESP_LOGCONFIG(TAG, "  ADC sample window: %" PRIu32 " us", sample_window_us_);
  ESP_LOGCONFIG(TAG, "  Stable CP time: %" PRIu32 " ms", stable_time_ms_);
  ESP_LOGCONFIG(TAG, "  Graceful stop timeout: %" PRIu32 " ms", graceful_stop_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Fault retry time: %" PRIu32 " ms", fault_retry_time_ms_);
  ESP_LOGCONFIG(TAG, "  A window: %u..%u mV", state_a_min_mv_, state_a_max_mv_);
  ESP_LOGCONFIG(TAG, "  B window: %u..%u mV", state_b_min_mv_, state_b_max_mv_);
  ESP_LOGCONFIG(TAG, "  C window: %u..%u mV", state_c_min_mv_, state_c_max_mv_);
  ESP_LOGCONFIG(TAG, "  D window: %u..%u mV", state_d_min_mv_, state_d_max_mv_);
  ESP_LOGCONFIG(TAG, "  -12 V diode window: %u..%u mV", diode_min_mv_, diode_max_mv_);
}

void EVSEComponent::loop() {
  const uint32_t now = millis();
  if ((uint32_t) (now - last_publish_ms_) >= 500) {
    last_publish_ms_ = now;
    publish_();
  }
}

void EVSEComponent::on_shutdown() {
#ifdef USE_ESP32
  if (task_handle_ != nullptr)
    vTaskSuspend(task_handle_);
#endif
  if (contactor_pin_ != nullptr)
    contactor_pin_->digital_write(false);
  if (pilot_output_ != nullptr)
    pilot_output_->set_level(0.0f);  // external CP driver -> -12 V fail-safe
}

void EVSEComponent::set_enabled(bool enabled) {
#ifdef USE_ESP32
  portENTER_CRITICAL(&data_mux_);
  requested_enabled_ = enabled;
  portEXIT_CRITICAL(&data_mux_);
#else
  requested_enabled_ = enabled;
#endif
  ESP_LOGI(TAG, "EVSE enable request = %s", YESNO(enabled));
}

bool EVSEComponent::is_enabled() {
#ifdef USE_ESP32
  portENTER_CRITICAL(&data_mux_);
  const bool value = requested_enabled_;
  portEXIT_CRITICAL(&data_mux_);
  return value;
#else
  return requested_enabled_;
#endif
}

void EVSEComponent::set_available(bool available) {
#ifdef USE_ESP32
  portENTER_CRITICAL(&data_mux_);
  requested_available_ = available;
  portEXIT_CRITICAL(&data_mux_);
#else
  requested_available_ = available;
#endif
  ESP_LOGI(TAG, "EVSE availability request = %s", YESNO(available));
}

bool EVSEComponent::is_available() {
#ifdef USE_ESP32
  portENTER_CRITICAL(&data_mux_);
  const bool value = requested_available_;
  portEXIT_CRITICAL(&data_mux_);
  return value;
#else
  return requested_available_;
#endif
}

void EVSEComponent::reset_fault() {
#ifdef USE_ESP32
  portENTER_CRITICAL(&data_mux_);
  reset_fault_requested_ = true;
  portEXIT_CRITICAL(&data_mux_);
#else
  reset_fault_requested_ = true;
#endif
  ESP_LOGI(TAG, "EVSE fault reset requested");
}

void EVSEComponent::set_current_limit(float amps) {
  if (amps < 6.0f)
    amps = 6.0f;
  if (amps > max_current_)
    amps = max_current_;

#ifdef USE_ESP32
  portENTER_CRITICAL(&data_mux_);
  requested_current_limit_ = amps;
  portEXIT_CRITICAL(&data_mux_);
#else
  requested_current_limit_ = amps;
#endif
  ESP_LOGI(TAG, "EVSE current limit request = %.1f A", amps);
}

float EVSEComponent::get_current_limit() {
#ifdef USE_ESP32
  portENTER_CRITICAL(&data_mux_);
  const float value = requested_current_limit_;
  portEXIT_CRITICAL(&data_mux_);
  return value;
#else
  return requested_current_limit_;
#endif
}

#ifdef USE_ESP32
void EVSEComponent::task_entry_(void *arg) {
  static_cast<EVSEComponent *>(arg)->task_loop_();
}

void EVSEComponent::task_loop_() {
  TickType_t last_wake = xTaskGetTickCount();
  TickType_t period_ticks = pdMS_TO_TICKS(sample_interval_ms_);
  if (period_ticks < 1)
    period_ticks = 1;

  const uint32_t period_us = sample_interval_ms_ * 1000UL;
  static constexpr uint32_t LATE_THRESHOLD_US = 1000UL;

  // Only used when timing_debug_ is enabled. Unsigned micros() arithmetic is
  // intentionally used so the normal ~71 minute micros() wrap is harmless.
  uint32_t expected_start_us = timing_debug_ ? micros() : 0;
  bool first_cycle = true;

  for (;;) {
    uint32_t cycle_start_us = 0;
    uint32_t lateness_us = 0;

    if (timing_debug_) {
      cycle_start_us = micros();

      if (!first_cycle) {
        const int32_t delta_us = static_cast<int32_t>(cycle_start_us - expected_start_us);
        if (delta_us > 0)
          lateness_us = static_cast<uint32_t>(delta_us);
      }
    }

    first_cycle = false;
    const uint32_t now = millis();

    process_requests_();
    sample_cp_();
    update_stable_cp_(sampled_cp_, now);
    update_diode_supervision_(now);
    control_(now);

    if (timing_debug_) {
      const uint32_t runtime_us = static_cast<uint32_t>(micros() - cycle_start_us);

      task_last_runtime_us_ = runtime_us;
      task_last_lateness_us_ = lateness_us;

      if (runtime_us > task_max_runtime_us_)
        task_max_runtime_us_ = runtime_us;
      if (lateness_us > task_max_lateness_us_)
        task_max_lateness_us_ = lateness_us;

      // "Late cycle" means the task started more than 1 ms after its nominal
      // release time. Runtime overruns are reported separately as deadlines.
      if (lateness_us > LATE_THRESHOLD_US)
        task_late_cycles_++;

      // A deadline is missed when this cycle completes at/after the next
      // nominal release point, whether due to late start, long runtime, or both.
      if (lateness_us + runtime_us >= period_us)
        task_missed_deadlines_++;

      expected_start_us += period_us;
    }

    update_snapshot_(now);
    vTaskDelayUntil(&last_wake, period_ticks);
  }
}
#endif

void EVSEComponent::process_requests_() {
  bool req_enabled;
  bool req_available;
  bool req_reset;
  float req_current;

#ifdef USE_ESP32
  portENTER_CRITICAL(&data_mux_);
  req_enabled = requested_enabled_;
  req_available = requested_available_;
  req_reset = reset_fault_requested_;
  reset_fault_requested_ = false;
  req_current = requested_current_limit_;
  portEXIT_CRITICAL(&data_mux_);
#else
  req_enabled = requested_enabled_;
  req_available = requested_available_;
  req_reset = reset_fault_requested_;
  reset_fault_requested_ = false;
  req_current = requested_current_limit_;
#endif

  if (enabled_ != req_enabled) {
    enabled_ = req_enabled;
    ESP_LOGI(TAG, "EVSE enabled = %s", YESNO(enabled_));
  }

  if (available_ != req_available) {
    available_ = req_available;
    ESP_LOGI(TAG, "EVSE available = %s", YESNO(available_));
  }

  if (current_limit_ != req_current) {
    current_limit_ = req_current;
    ESP_LOGI(TAG, "Applied current limit = %.1f A", current_limit_);
    if (pilot_mode_ == PilotMode::PWM)
      apply_pilot_output_();
  }

  if (req_reset && fault_code_ != FaultCode::NONE) {
    ESP_LOGI(TAG, "Manual fault reset");
    clear_fault_(millis());
  }
}

float EVSEComponent::duty_for_current_(float amps) const {
  float duty_percent = amps / 0.6f;
  if (duty_percent < 10.0f)
    duty_percent = 10.0f;
  if (duty_percent > 85.0f)
    duty_percent = 85.0f;
  return duty_percent / 100.0f;
}

bool EVSEComponent::mv_in_window_(uint16_t v, uint16_t lo, uint16_t hi) const {
  return v >= lo && v <= hi;
}

void EVSEComponent::sample_cp_() {
#ifdef USE_ARDUINO
  uint32_t low_mv = UINT32_MAX;
  uint32_t high_mv = 0;
  const uint32_t start = micros();

  // analogReadMilliVolts() uses the Arduino-ESP32 calibrated one-shot ADC
  // conversion. The window is deliberately > 2 CP periods so the 10% positive
  // pulse at the 6 A minimum current is sampled reliably.
  while ((uint32_t) (micros() - start) < sample_window_us_) {
    const uint32_t sample_mv = analogReadMilliVolts(pilot_adc_gpio_num_);
    if (sample_mv < low_mv)
      low_mv = sample_mv;
    if (sample_mv > high_mv)
      high_mv = sample_mv;
  }

  if (low_mv == UINT32_MAX)
    low_mv = 0;

  cp_high_mv_ = static_cast<uint16_t>(high_mv > 65535U ? 65535U : high_mv);
  cp_low_mv_ = static_cast<uint16_t>(low_mv > 65535U ? 65535U : low_mv);
  sampled_cp_ = classify_cp_(cp_high_mv_);
  diode_sample_valid_ =
      pilot_mode_ != PilotMode::PWM || mv_in_window_(cp_low_mv_, diode_min_mv_, diode_max_mv_);
#endif
}

CpLevel EVSEComponent::classify_cp_(uint16_t high_mv) const {
  if (mv_in_window_(high_mv, state_a_min_mv_, state_a_max_mv_))
    return CpLevel::A;
  if (mv_in_window_(high_mv, state_b_min_mv_, state_b_max_mv_))
    return CpLevel::B;
  if (mv_in_window_(high_mv, state_c_min_mv_, state_c_max_mv_))
    return CpLevel::C;
  if (mv_in_window_(high_mv, state_d_min_mv_, state_d_max_mv_))
    return CpLevel::D;
  return CpLevel::INVALID;
}

void EVSEComponent::update_stable_cp_(CpLevel sampled, uint32_t now) {
  if (sampled != candidate_cp_) {
    candidate_cp_ = sampled;
    candidate_since_ms_ = now;
    return;
  }

  if (sampled != stable_cp_ && (uint32_t) (now - candidate_since_ms_) >= stable_time_ms_) {
    stable_cp_ = sampled;
    ESP_LOGI(TAG, "Stable CP -> %s (high=%u mV low=%u mV)", cp_level_to_string_(stable_cp_), cp_high_mv_, cp_low_mv_);
  }
}

void EVSEComponent::update_diode_supervision_(uint32_t now) {
  if (pilot_mode_ != PilotMode::PWM) {
    diode_invalid_since_ms_ = 0;
    diode_valid_ = true;
    return;
  }

  if (diode_sample_valid_) {
    diode_invalid_since_ms_ = 0;
    diode_valid_ = true;
    return;
  }

  diode_valid_ = false;
  if (diode_invalid_since_ms_ == 0) {
    diode_invalid_since_ms_ = now;
    return;
  }

  if ((uint32_t) (now - diode_invalid_since_ms_) >= diode_fault_time_ms_)
    raise_fault_(FaultCode::DIODE_FAULT);
}

EvseState EVSEComponent::target_state_for_cp_(CpLevel cp, bool charging_allowed) {
  switch (state_) {
    case EvseState::A:
      if (cp == CpLevel::A) return EvseState::A;
      if (cp == CpLevel::B) return charging_allowed ? EvseState::B2 : EvseState::B1;
      break;

    case EvseState::B1:
    case EvseState::B2:
      if (cp == CpLevel::A) return EvseState::A;
      if (cp == CpLevel::B) return charging_allowed ? EvseState::B2 : EvseState::B1;
      if (cp == CpLevel::C) return charging_allowed ? EvseState::C2 : EvseState::C1;
      break;

    case EvseState::C1:
    case EvseState::C2:
      if (cp == CpLevel::A) return EvseState::A;
      if (cp == CpLevel::B) return charging_allowed ? EvseState::B2 : EvseState::B1;
      if (cp == CpLevel::C) return charging_allowed ? EvseState::C2 : EvseState::C1;
      if (cp == CpLevel::D)
        return (charging_allowed && allow_ventilation_) ? EvseState::D2 : EvseState::D1;
      break;

    case EvseState::D1:
    case EvseState::D2:
      if (cp == CpLevel::C) return charging_allowed ? EvseState::C2 : EvseState::C1;
      if (cp == CpLevel::D)
        return (charging_allowed && allow_ventilation_) ? EvseState::D2 : EvseState::D1;
      break;

    case EvseState::E:
    case EvseState::F:
      return state_;
  }

  return EvseState::E;
}

void EVSEComponent::control_(uint32_t now) {
  if (fault_code_ != FaultCode::NONE) {
    if (state_ != EvseState::E)
      enter_state_(EvseState::E, now);

    if ((uint32_t) (now - fault_since_ms_) >= fault_retry_time_ms_) {
      ESP_LOGI(TAG, "Auto-clearing transient EVSE fault after %" PRIu32 " ms", fault_retry_time_ms_);
      clear_fault_(now);
    }
    return;
  }

  if (!available_) {
    if (state_ != EvseState::F)
      enter_state_(EvseState::F, now);
    return;
  }

  if (state_ == EvseState::F) {
    stable_cp_ = CpLevel::UNKNOWN;
    candidate_cp_ = CpLevel::UNKNOWN;
    enter_state_(EvseState::A, now);
    return;
  }

  if (stable_cp_ == CpLevel::UNKNOWN) {
    service_state_actions_(now);
    return;
  }

  if (stable_cp_ == CpLevel::INVALID) {
    raise_fault_(FaultCode::PILOT_VOLTAGE);
    return;
  }

  const EvseState target = target_state_for_cp_(stable_cp_, enabled_);
  if (target == EvseState::E) {
    raise_fault_(FaultCode::PILOT_TRANSITION);
    return;
  }

  if (target != state_)
    enter_state_(target, now);

  service_state_actions_(now);
}

bool EVSEComponent::is_energizing_state_(EvseState s) const {
  return s == EvseState::C2 || s == EvseState::D2;
}

bool EVSEComponent::is_paused_active_state_(EvseState s) const {
  return s == EvseState::C1 || s == EvseState::D1;
}

void EVSEComponent::enter_state_(EvseState next, uint32_t now) {
  if (next == state_)
    return;

  const EvseState previous = state_;
  const bool was_energizing = is_energizing_state_(previous) && contactor_on_;
  state_ = next;
  state_entered_ms_ = now;

  ESP_LOGI(TAG, "EVSE state: %s -> %s", state_to_string_(previous), state_to_string_(next));

  switch (next) {
    case EvseState::A:
      graceful_stop_active_ = false;
      open_contactor_();
      set_pilot_mode_(PilotMode::POSITIVE_DC);
      break;

    case EvseState::B1:
      graceful_stop_active_ = false;
      open_contactor_();
      set_pilot_mode_(PilotMode::POSITIVE_DC);
      break;

    case EvseState::B2:
      graceful_stop_active_ = false;
      open_contactor_();
      set_pilot_mode_(PilotMode::PWM);
      break;

    case EvseState::C1:
    case EvseState::D1:
      set_pilot_mode_(PilotMode::POSITIVE_DC);
      if (was_energizing) {
        graceful_stop_active_ = true;
        graceful_stop_started_ms_ = now;
        ESP_LOGI(TAG, "Graceful stop started; contactor held for up to %" PRIu32 " ms", graceful_stop_timeout_ms_);
      } else {
        graceful_stop_active_ = false;
        open_contactor_();
      }
      break;

    case EvseState::C2:
    case EvseState::D2:
      graceful_stop_active_ = false;
      set_pilot_mode_(PilotMode::PWM);
      break;

    case EvseState::E:
    case EvseState::F:
      graceful_stop_active_ = false;
      open_contactor_();
      // Fail-safe output with the current one-bit CP driver architecture.
      // This corresponds electrically to -12 V (F-like unavailable output).
      set_pilot_mode_(PilotMode::NEGATIVE_DC);
      break;
  }
}

void EVSEComponent::service_state_actions_(uint32_t now) {
  if (is_paused_active_state_(state_) && graceful_stop_active_) {
    if ((uint32_t) (now - graceful_stop_started_ms_) >= graceful_stop_timeout_ms_) {
      ESP_LOGW(TAG, "Graceful stop timeout; forcing contactor open");
      open_contactor_();
      graceful_stop_active_ = false;
    }
  }

  if (is_energizing_state_(state_) && !contactor_on_) {
    if (!diode_valid_)
      return;

    if ((uint32_t) (now - state_entered_ms_) >= contactor_close_delay_ms_)
      close_contactor_();
  }
}

void EVSEComponent::set_pilot_mode_(PilotMode mode) {
  if (pilot_mode_ == mode)
    return;

  pilot_mode_ = mode;
  apply_pilot_output_();

  const char *name = mode == PilotMode::POSITIVE_DC ? "+12 V DC" :
                     mode == PilotMode::PWM ? "1 kHz PWM" : "-12 V DC";
  ESP_LOGI(TAG, "Pilot mode -> %s", name);
}

void EVSEComponent::apply_pilot_output_() {
  if (pilot_output_ == nullptr)
    return;

  switch (pilot_mode_) {
    case PilotMode::POSITIVE_DC:
      pilot_output_->set_level(1.0f);
      break;
    case PilotMode::PWM:
      pilot_output_->set_level(duty_for_current_(current_limit_));
      break;
    case PilotMode::NEGATIVE_DC:
      pilot_output_->set_level(0.0f);
      break;
  }
}

void EVSEComponent::open_contactor_() {
  if (contactor_pin_ != nullptr)
    contactor_pin_->digital_write(false);

  if (contactor_on_) {
    contactor_on_ = false;
    ESP_LOGI(TAG, "Contactor OFF");
  }
}

void EVSEComponent::close_contactor_() {
  if (fault_code_ != FaultCode::NONE || !available_ || !enabled_ || !is_energizing_state_(state_))
    return;
  if (!diode_valid_)
    return;

  if (contactor_pin_ != nullptr)
    contactor_pin_->digital_write(true);

  if (!contactor_on_) {
    contactor_on_ = true;
    ESP_LOGI(TAG, "Contactor ON");
  }
}

void EVSEComponent::raise_fault_(FaultCode code) {
  if (fault_code_ != FaultCode::NONE)
    return;

  fault_code_ = code;
  fault_since_ms_ = millis();
  ESP_LOGE(TAG, "FAULT: %s", fault_to_string_(code));
  enter_state_(EvseState::E, fault_since_ms_);
}

void EVSEComponent::clear_fault_(uint32_t now) {
  fault_code_ = FaultCode::NONE;
  diode_invalid_since_ms_ = 0;
  diode_valid_ = true;
  stable_cp_ = CpLevel::UNKNOWN;
  candidate_cp_ = CpLevel::UNKNOWN;

  if (!available_)
    enter_state_(EvseState::F, now);
  else
    enter_state_(EvseState::A, now);
}

const char *EVSEComponent::cp_level_to_string_(CpLevel s) const {
  switch (s) {
    case CpLevel::A: return "A (~12 V)";
    case CpLevel::B: return "B (~9 V)";
    case CpLevel::C: return "C (~6 V)";
    case CpLevel::D: return "D (~3 V)";
    case CpLevel::INVALID: return "Invalid";
    default: return "Unknown";
  }
}

const char *EVSEComponent::state_to_string_(EvseState s) const {
  switch (s) {
    case EvseState::A: return "A - Disconnected";
    case EvseState::B1: return "B1 - Connected / not offering energy";
    case EvseState::B2: return "B2 - Connected / PWM active";
    case EvseState::C1: return "C1 - Charge requested / paused";
    case EvseState::C2: return "C2 - Charging";
    case EvseState::D1: return "D1 - Ventilation requested / paused";
    case EvseState::D2: return "D2 - Charging with ventilation";
    case EvseState::E: return "E - Error";
    case EvseState::F: return "F - EVSE unavailable";
    default: return "Unknown";
  }
}

const char *EVSEComponent::fault_to_string_(FaultCode code) const {
  switch (code) {
    case FaultCode::NONE: return "None";
    case FaultCode::PILOT_VOLTAGE: return "CP voltage outside valid A/B/C/D windows";
    case FaultCode::PILOT_TRANSITION: return "Invalid CP transition for current EVSE state";
    case FaultCode::DIODE_FAULT: return "CP diode check failed";
    default: return "Unknown fault";
  }
}

void EVSEComponent::update_snapshot_(uint32_t heartbeat_ms) {
#ifdef USE_ESP32
  portENTER_CRITICAL(&data_mux_);
#endif
  snapshot_state_ = state_;
  snapshot_cp_ = stable_cp_;
  snapshot_fault_ = fault_code_;
  snapshot_cp_high_mv_ = cp_high_mv_;
  snapshot_cp_low_mv_ = cp_low_mv_;
  snapshot_advertised_current_ = pilot_mode_ == PilotMode::PWM ? current_limit_ : 0.0f;
  snapshot_contactor_on_ = contactor_on_;
  snapshot_graceful_stop_ = graceful_stop_active_;
  snapshot_task_heartbeat_ms_ = heartbeat_ms;
  snapshot_task_late_cycles_ = task_late_cycles_;
  snapshot_task_last_runtime_us_ = task_last_runtime_us_;
  snapshot_task_max_runtime_us_ = task_max_runtime_us_;
  snapshot_task_last_lateness_us_ = task_last_lateness_us_;
  snapshot_task_max_lateness_us_ = task_max_lateness_us_;
  snapshot_task_missed_deadlines_ = task_missed_deadlines_;
#ifdef USE_ESP32
  portEXIT_CRITICAL(&data_mux_);
#endif
}

void EVSEComponent::publish_() {
  EvseState state;
  CpLevel cp;
  FaultCode fault;
  uint16_t high_mv;
  uint16_t low_mv;
  float advertised_current;
  bool contactor_on;
  bool graceful_stop;
  uint32_t heartbeat_ms;
  uint32_t late_cycles;
  uint32_t last_runtime_us;
  uint32_t max_runtime_us;
  uint32_t last_lateness_us;
  uint32_t max_lateness_us;
  uint32_t missed_deadlines;

#ifdef USE_ESP32
  portENTER_CRITICAL(&data_mux_);
#endif
  state = snapshot_state_;
  cp = snapshot_cp_;
  fault = snapshot_fault_;
  high_mv = snapshot_cp_high_mv_;
  low_mv = snapshot_cp_low_mv_;
  advertised_current = snapshot_advertised_current_;
  contactor_on = snapshot_contactor_on_;
  graceful_stop = snapshot_graceful_stop_;
  heartbeat_ms = snapshot_task_heartbeat_ms_;
  late_cycles = snapshot_task_late_cycles_;
  last_runtime_us = snapshot_task_last_runtime_us_;
  max_runtime_us = snapshot_task_max_runtime_us_;
  last_lateness_us = snapshot_task_last_lateness_us_;
  max_lateness_us = snapshot_task_max_lateness_us_;
  missed_deadlines = snapshot_task_missed_deadlines_;
#ifdef USE_ESP32
  portEXIT_CRITICAL(&data_mux_);
#endif

  if (state_sensor_ != nullptr)
    state_sensor_->publish_state(state_to_string_(state));
  if (physical_state_sensor_ != nullptr)
    physical_state_sensor_->publish_state(cp_level_to_string_(cp));
  if (fault_reason_sensor_ != nullptr)
    fault_reason_sensor_->publish_state(fault_to_string_(fault));
  if (cp_high_mv_sensor_ != nullptr)
    cp_high_mv_sensor_->publish_state(high_mv);
  if (cp_low_mv_sensor_ != nullptr)
    cp_low_mv_sensor_->publish_state(low_mv);
  if (advertised_current_sensor_ != nullptr)
    advertised_current_sensor_->publish_state(advertised_current);
  if (task_late_cycles_sensor_ != nullptr)
    task_late_cycles_sensor_->publish_state(late_cycles);
  if (task_max_runtime_sensor_ != nullptr)
    task_max_runtime_sensor_->publish_state(max_runtime_us);
  if (task_last_runtime_sensor_ != nullptr)
    task_last_runtime_sensor_->publish_state(last_runtime_us);
  if (task_last_lateness_sensor_ != nullptr)
    task_last_lateness_sensor_->publish_state(last_lateness_us);
  if (task_max_lateness_sensor_ != nullptr)
    task_max_lateness_sensor_->publish_state(max_lateness_us);
  if (task_missed_deadlines_sensor_ != nullptr)
    task_missed_deadlines_sensor_->publish_state(missed_deadlines);

  const bool connected = state == EvseState::B1 || state == EvseState::B2 ||
                         state == EvseState::C1 || state == EvseState::C2 ||
                         state == EvseState::D1 || state == EvseState::D2;
  if (vehicle_connected_sensor_ != nullptr)
    vehicle_connected_sensor_->publish_state(connected);
  if (charging_sensor_ != nullptr)
    charging_sensor_->publish_state((state == EvseState::C2 || state == EvseState::D2) && contactor_on);
  if (stopping_sensor_ != nullptr)
    stopping_sensor_->publish_state(graceful_stop);
  if (fault_sensor_ != nullptr)
    fault_sensor_->publish_state(fault != FaultCode::NONE);

  if (task_running_sensor_ != nullptr) {
    const uint32_t now = millis();
    const uint32_t timeout_ms = sample_interval_ms_ * 10U < 500U ? 500U : sample_interval_ms_ * 10U;
    const bool task_running = heartbeat_ms != 0 && (uint32_t) (now - heartbeat_ms) <= timeout_ms;
    task_running_sensor_->publish_state(task_running);
  }
}

}  // namespace evse
}  // namespace esphome

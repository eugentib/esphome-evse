#include "evse.h"
#include <inttypes.h>

#ifdef USE_ARDUINO
#include <Arduino.h>
#endif

namespace esphome {
namespace evse {

static const char *const TAG = "evse";

void EVSEComponent::setup() {
#ifdef USE_ARDUINO
  if (pilot_adc_pin_ == nullptr) {
    ESP_LOGE(TAG, "Pilot ADC pin is not configured");
    mark_failed();
    return;
  }
  pilot_adc_pin_->setup();
  pilot_adc_gpio_num_ = pilot_adc_pin_->get_pin();
  analogReadResolution(12);
#else
  ESP_LOGE(TAG, "v0.2.1 currently requires the Arduino framework");
  mark_failed();
  return;
#endif

  enabled_ = false;
  available_ = true;
  state_ = EvseState::A;
  fault_code_ = FaultCode::NONE;
  fault_reason_ = "None";
  graceful_stop_active_ = false;

  open_contactor_();
  set_pilot_mode_(PilotMode::POSITIVE_DC);

  const uint32_t now = millis();
  candidate_since_ms_ = now;
  state_entered_ms_ = now;

  ESP_LOGI(TAG, "EVSE v0.2.1 initialized; ADC=GPIO%u; enabled=OFF; available=ON", pilot_adc_gpio_num_);
}

void EVSEComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ESPHome EVSE v0.2.1:");
  LOG_PIN("  Pilot ADC Pin: ", pilot_adc_pin_);
  ESP_LOGCONFIG(TAG, "  Max/default current: %.1f / %.1f A", max_current_, default_current_);
  ESP_LOGCONFIG(TAG, "  Allow State D charging: %s", YESNO(allow_ventilation_));
  ESP_LOGCONFIG(TAG, "  Stable CP time: %" PRIu32 " ms", stable_time_ms_);
  ESP_LOGCONFIG(TAG, "  Graceful stop timeout: %" PRIu32 " ms", graceful_stop_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Fault retry time: %" PRIu32 " ms", fault_retry_time_ms_);
  ESP_LOGCONFIG(TAG, "  A window: %u..%u", state_a_min_raw_, state_a_max_raw_);
  ESP_LOGCONFIG(TAG, "  B window: %u..%u", state_b_min_raw_, state_b_max_raw_);
  ESP_LOGCONFIG(TAG, "  C window: %u..%u", state_c_min_raw_, state_c_max_raw_);
  ESP_LOGCONFIG(TAG, "  D window: %u..%u", state_d_min_raw_, state_d_max_raw_);
  ESP_LOGCONFIG(TAG, "  -12 V diode window: %u..%u", diode_min_raw_, diode_max_raw_);
}

void EVSEComponent::loop() {
  const uint32_t now = millis();

  if ((uint32_t) (now - last_sample_ms_) >= sample_interval_ms_) {
    last_sample_ms_ = now;
    sample_cp_();
    update_stable_cp_(sampled_cp_);
    update_diode_supervision_(now);
    control_();
  }

  if ((uint32_t) (now - last_publish_ms_) >= 500) {
    last_publish_ms_ = now;
    publish_();
  }
}

void EVSEComponent::set_enabled(bool enabled) {
  enabled_ = enabled;
  ESP_LOGI(TAG, "EVSE enabled = %s", YESNO(enabled));
  // Do not open the contactor here. If charging is active, control_() moves
  // C2/D2 -> C1/D1 and performs the IEC-style graceful stop sequence.
}

void EVSEComponent::set_available(bool available) {
  if (available_ == available)
    return;

  available_ = available;
  ESP_LOGI(TAG, "EVSE available = %s", YESNO(available));

  if (!available_) {
    if (fault_code_ == FaultCode::NONE)
      enter_state_(EvseState::F);
  } else if (fault_code_ == FaultCode::NONE) {
    // Re-enter service from State A. Fresh CP samples will then move to B/C.
    stable_cp_ = CpLevel::UNKNOWN;
    candidate_cp_ = CpLevel::UNKNOWN;
    enter_state_(EvseState::A);
  }
}

void EVSEComponent::reset_fault() {
  if (fault_code_ == FaultCode::NONE)
    return;

  ESP_LOGI(TAG, "Manual fault reset");
  clear_fault_();
}

void EVSEComponent::set_current_limit(float amps) {
  if (amps < 6.0f)
    amps = 6.0f;
  if (amps > max_current_)
    amps = max_current_;

  current_limit_ = amps;
  ESP_LOGI(TAG, "Current limit = %.1f A", current_limit_);

  if (pilot_mode_ == PilotMode::PWM)
    apply_pilot_output_();
}

float EVSEComponent::duty_for_current_(float amps) const {
  // IEC 61851 basic AC range used by this project: 6..32 A.
  // I(A) = duty(%) * 0.6 for 10%..85% duty.
  float duty_percent = amps / 0.6f;
  if (duty_percent < 10.0f)
    duty_percent = 10.0f;
  if (duty_percent > 85.0f)
    duty_percent = 85.0f;
  return duty_percent / 100.0f;
}

bool EVSEComponent::raw_in_window_(uint16_t v, uint16_t lo, uint16_t hi) const {
  return v >= lo && v <= hi;
}

void EVSEComponent::sample_cp_() {
#ifdef USE_ARDUINO
  uint16_t low = 4095;
  uint16_t high = 0;
  const uint32_t start = micros();

  // >1 ms captures at least one complete 1 kHz CP period.
  while ((uint32_t) (micros() - start) < sample_window_us_) {
    const int sample = analogRead(pilot_adc_gpio_num_);
    if (sample < low)
      low = sample;
    if (sample > high)
      high = sample;
  }

  cp_high_raw_ = high;
  cp_low_raw_ = low;
  sampled_cp_ = classify_cp_(high);
  diode_sample_valid_ =
      pilot_mode_ != PilotMode::PWM || raw_in_window_(low, diode_min_raw_, diode_max_raw_);
#endif
}

CpLevel EVSEComponent::classify_cp_(uint16_t high_raw) const {
  if (raw_in_window_(high_raw, state_a_min_raw_, state_a_max_raw_))
    return CpLevel::A;
  if (raw_in_window_(high_raw, state_b_min_raw_, state_b_max_raw_))
    return CpLevel::B;
  if (raw_in_window_(high_raw, state_c_min_raw_, state_c_max_raw_))
    return CpLevel::C;
  if (raw_in_window_(high_raw, state_d_min_raw_, state_d_max_raw_))
    return CpLevel::D;
  return CpLevel::INVALID;
}

void EVSEComponent::update_stable_cp_(CpLevel sampled) {
  const uint32_t now = millis();

  if (sampled != candidate_cp_) {
    candidate_cp_ = sampled;
    candidate_since_ms_ = now;
    return;
  }

  if (sampled != stable_cp_ && (uint32_t) (now - candidate_since_ms_) >= stable_time_ms_) {
    stable_cp_ = sampled;
    ESP_LOGI(TAG, "Stable CP -> %s (high=%u low=%u)", cp_level_to_string_(stable_cp_), cp_high_raw_, cp_low_raw_);
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

  if ((uint32_t) (now - diode_invalid_since_ms_) >= diode_fault_time_ms_) {
    raise_fault_(FaultCode::DIODE_FAULT, "CP diode check failed");
  }
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
      // A direct B -> D transition is considered invalid.
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
      // E/F are handled before this transition table.
      return state_;
  }

  return EvseState::E;  // Sentinel: caller raises PILOT_FAULT.
}

void EVSEComponent::control_() {
  const uint32_t now = millis();

  if (fault_code_ != FaultCode::NONE) {
    if (state_ != EvseState::E)
      enter_state_(EvseState::E);
    if ((uint32_t) (now - fault_since_ms_) >= fault_retry_time_ms_) {
      ESP_LOGI(TAG, "Auto-clearing transient EVSE fault after %" PRIu32 " ms", fault_retry_time_ms_);
      clear_fault_();
    }
    return;
  }

  if (!available_) {
    if (state_ != EvseState::F)
      enter_state_(EvseState::F);
    return;
  }

  if (stable_cp_ == CpLevel::UNKNOWN) {
    service_state_actions_(now);
    return;
  }

  if (stable_cp_ == CpLevel::INVALID) {
    raise_fault_(FaultCode::PILOT_FAULT, "CP voltage outside valid A/B/C/D windows");
    return;
  }

  const bool charging_allowed = enabled_;
  const EvseState target = target_state_for_cp_(stable_cp_, charging_allowed);

  if (target == EvseState::E) {
    raise_fault_(FaultCode::PILOT_FAULT, "Invalid CP transition for current EVSE state");
    return;
  }

  if (target != state_)
    enter_state_(target);

  service_state_actions_(now);
}

bool EVSEComponent::is_energizing_state_(EvseState s) const {
  return s == EvseState::C2 || s == EvseState::D2;
}

bool EVSEComponent::is_paused_active_state_(EvseState s) const {
  return s == EvseState::C1 || s == EvseState::D1;
}

void EVSEComponent::enter_state_(EvseState next) {
  if (next == state_)
    return;

  const EvseState previous = state_;
  const bool was_energizing = is_energizing_state_(previous) && contactor_on_;
  state_ = next;
  state_entered_ms_ = millis();

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
      // Pause charging by suppressing PWM first. If we came from an energized
      // C2/D2 state, keep the contactor closed while the EV winds current down.
      set_pilot_mode_(PilotMode::POSITIVE_DC);
      if (was_energizing) {
        graceful_stop_active_ = true;
        graceful_stop_started_ms_ = state_entered_ms_;
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
      // If the contactor remained closed during a short pause, leave it closed.
      break;

    case EvseState::E:
    case EvseState::F:
      graceful_stop_active_ = false;
      open_contactor_();
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
  if (pilot_mode_ == mode) {
    apply_pilot_output_();
    return;
  }

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
      pilot_output_->set_level(1.0f);  // external driver -> +12 V
      break;
    case PilotMode::PWM:
      pilot_output_->set_level(duty_for_current_(current_limit_));
      break;
    case PilotMode::NEGATIVE_DC:
      pilot_output_->set_level(0.0f);  // external driver -> -12 V
      break;
  }
}

void EVSEComponent::open_contactor_() {
  if (contactor_ != nullptr)
    contactor_->turn_off();

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

  if (contactor_ != nullptr)
    contactor_->turn_on();

  if (!contactor_on_) {
    contactor_on_ = true;
    ESP_LOGI(TAG, "Contactor ON");
  }
}

void EVSEComponent::raise_fault_(FaultCode code, const char *reason) {
  if (fault_code_ != FaultCode::NONE)
    return;

  fault_code_ = code;
  fault_reason_ = reason != nullptr ? reason : "EVSE fault";
  fault_since_ms_ = millis();

  ESP_LOGE(TAG, "FAULT: %s", fault_reason_.c_str());
  enter_state_(EvseState::E);
}

void EVSEComponent::clear_fault_() {
  fault_code_ = FaultCode::NONE;
  fault_reason_ = "None";
  diode_invalid_since_ms_ = 0;
  diode_valid_ = true;
  stable_cp_ = CpLevel::UNKNOWN;
  candidate_cp_ = CpLevel::UNKNOWN;

  if (!available_)
    enter_state_(EvseState::F);
  else
    enter_state_(EvseState::A);
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

const char *EVSEComponent::fault_to_string_() const {
  return fault_reason_.c_str();
}

void EVSEComponent::publish_() {
  if (state_sensor_ != nullptr)
    state_sensor_->publish_state(state_to_string_(state_));
  if (physical_state_sensor_ != nullptr)
    physical_state_sensor_->publish_state(cp_level_to_string_(stable_cp_));
  if (fault_reason_sensor_ != nullptr)
    fault_reason_sensor_->publish_state(fault_to_string_());
  if (cp_high_raw_sensor_ != nullptr)
    cp_high_raw_sensor_->publish_state(cp_high_raw_);
  if (cp_low_raw_sensor_ != nullptr)
    cp_low_raw_sensor_->publish_state(cp_low_raw_);
  if (advertised_current_sensor_ != nullptr)
    advertised_current_sensor_->publish_state(pilot_mode_ == PilotMode::PWM ? current_limit_ : 0.0f);

  const bool connected = state_ == EvseState::B1 || state_ == EvseState::B2 ||
                         state_ == EvseState::C1 || state_ == EvseState::C2 ||
                         state_ == EvseState::D1 || state_ == EvseState::D2;
  if (vehicle_connected_sensor_ != nullptr)
    vehicle_connected_sensor_->publish_state(connected);
  if (charging_sensor_ != nullptr)
    charging_sensor_->publish_state(is_energizing_state_(state_) && contactor_on_);
  if (stopping_sensor_ != nullptr)
    stopping_sensor_->publish_state(graceful_stop_active_);
  if (fault_sensor_ != nullptr)
    fault_sensor_->publish_state(fault_code_ != FaultCode::NONE);
}

}  // namespace evse
}  // namespace esphome

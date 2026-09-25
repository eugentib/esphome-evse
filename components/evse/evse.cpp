#include "evse.h"

namespace esphome {
namespace evse {

static const char *const TAG = "evse";

void EVSEComponent::setup() {
#ifndef USE_ESP32
  ESP_LOGE(TAG, "v0.4.8 requires ESP32");
  mark_failed();
  return;
#else
  if (pilot_pwm_pin_ == nullptr || pilot_adc_pin_ == nullptr || contactor_pin_ == nullptr) {
    ESP_LOGE(TAG, "Pilot PWM pin, pilot ADC pin and contactor pin must all be configured");
    mark_failed();
    return;
  }

  pilot_pwm_pin_->setup();
  pilot_pwm_gpio_num_ = pilot_pwm_pin_->get_pin();
  if (!setup_pilot_pwm_()) {
    ESP_LOGE(TAG, "Failed to initialize native ESP-IDF LEDC on GPIO%u", pilot_pwm_gpio_num_);
    contactor_pin_->setup();
    contactor_pin_->digital_write(false);
    mark_failed();
    return;
  }

  pilot_adc_pin_->setup();
  pilot_adc_gpio_num_ = pilot_adc_pin_->get_pin();

  if (!setup_adc_dma_()) {
    ESP_LOGE(TAG, "Failed to initialize continuous ADC/DMA on GPIO%u", pilot_adc_gpio_num_);
    contactor_pin_->setup();
    contactor_pin_->digital_write(false);
    set_pilot_static_(false);
    shutdown_pilot_pwm_();
    mark_failed();
    return;
  }

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
  if (!apply_pilot_output_()) {
    ESP_LOGE(TAG, "Failed to apply initial +12 V CP state");
    contactor_pin_->digital_write(false);
    shutdown_adc_dma_();
    shutdown_pilot_pwm_();
    mark_failed();
    return;
  }

  const uint32_t now = now_ms_();
  candidate_since_ms_ = now;
  invalid_since_ms_ = 0;
  state_entered_ms_ = now;
  update_snapshot_(now);

#ifdef USE_OTA_STATE_LISTENER
  ota::get_global_ota_callback()->add_global_state_listener(this);
#endif

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
    set_pilot_static_(false);
    shutdown_adc_dma_();
    shutdown_pilot_pwm_();
    mark_failed();
    return;
  }

  ESP_LOGI(
      TAG,
      "EVSE v0.4.8 initialized; PWM=GPIO%u ADC=GPIO%u, task core=%u priority=%u stack=%" PRIu32 " B",
      pilot_pwm_gpio_num_,
      pilot_adc_gpio_num_,
      task_core_,
      task_priority_,
      task_stack_size_
  );
#endif
}

void EVSEComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "ESPHome EVSE v0.4.8:");
  LOG_PIN("  Pilot PWM Pin: ", pilot_pwm_pin_);
  LOG_PIN("  Pilot ADC Pin: ", pilot_adc_pin_);
  LOG_PIN("  Contactor Pin: ", contactor_pin_);
  ESP_LOGCONFIG(TAG, "  Max/default current: %.1f / %.1f A", max_current_, default_current_);
  ESP_LOGCONFIG(TAG, "  Allow State D charging: %s", YESNO(allow_ventilation_));
  ESP_LOGCONFIG(TAG, "  EVSE task: core=%u priority=%u stack=%" PRIu32 " B", task_core_, task_priority_, task_stack_size_);
  ESP_LOGCONFIG(TAG, "  Timing debug: %s", YESNO(timing_debug_));
  ESP_LOGCONFIG(TAG, "  Task/sample interval: %" PRIu32 " ms", sample_interval_ms_);
  ESP_LOGCONFIG(TAG, "  Pilot PWM: native ESP-IDF LEDC, 1 kHz");
  ESP_LOGCONFIG(TAG, "  OTA safety interlock: enabled");
  ESP_LOGCONFIG(TAG, "  ADC mode: continuous DMA");
  ESP_LOGCONFIG(TAG, "  ADC sample rate: %" PRIu32 " samples/s", adc_sample_rate_hz_);
  ESP_LOGCONFIG(TAG, "  ADC peak averaging: %u samples", adc_peak_samples_);
  ESP_LOGCONFIG(TAG, "  ADC minimum samples/cycle: %u", adc_min_samples_);
  ESP_LOGCONFIG(TAG, "  ADC fault time: %" PRIu32 " ms", adc_fault_time_ms_);
  ESP_LOGCONFIG(TAG, "  CP confirmation windows: %u", cp_confirm_windows_);
  ESP_LOGCONFIG(TAG, "  Stable CP time: %" PRIu32 " ms", stable_time_ms_);
  ESP_LOGCONFIG(TAG, "  Invalid CP grace time: %" PRIu32 " ms", invalid_grace_time_ms_);
  ESP_LOGCONFIG(TAG, "  Graceful stop timeout: %" PRIu32 " ms", graceful_stop_timeout_ms_);
  ESP_LOGCONFIG(TAG, "  Fault retry time: %" PRIu32 " ms", fault_retry_time_ms_);
  ESP_LOGCONFIG(TAG, "  A window: %u..%u mV", state_a_min_mv_, state_a_max_mv_);
  ESP_LOGCONFIG(TAG, "  B window: %u..%u mV", state_b_min_mv_, state_b_max_mv_);
  ESP_LOGCONFIG(TAG, "  C window: %u..%u mV", state_c_min_mv_, state_c_max_mv_);
  ESP_LOGCONFIG(TAG, "  D window: %u..%u mV", state_d_min_mv_, state_d_max_mv_);
  ESP_LOGCONFIG(TAG, "  -12 V diode window: %u..%u mV", diode_min_mv_, diode_max_mv_);
}

void EVSEComponent::loop() {
  const uint32_t now = now_ms_();
  if ((uint32_t) (now - last_publish_ms_) >= 500) {
    last_publish_ms_ = now;
    publish_();
  }
}

void EVSEComponent::on_shutdown() {
  ota_lockout_.store(true, std::memory_order_release);
#ifdef USE_ESP32
  if (task_handle_ != nullptr)
    vTaskSuspend(task_handle_);
#endif
  force_safe_outputs_();
#ifdef USE_ESP32
  shutdown_adc_dma_();
  shutdown_pilot_pwm_();
#endif
}

#ifdef USE_OTA_STATE_LISTENER
void EVSEComponent::on_ota_global_state(ota::OTAState ota_state, float progress, uint8_t error,
                                        ota::OTAComponent *component) {
  (void) progress;
  (void) error;
  (void) component;

  if (ota_state == ota::OTA_STARTED) {
    // OTA can block the normal application loop. Enter the safe hardware state
    // synchronously before the data transfer begins.
    ota_lockout_.store(true, std::memory_order_release);

#ifdef USE_ESP32
    portENTER_CRITICAL(&data_mux_);
    requested_enabled_ = false;
    portEXIT_CRITICAL(&data_mux_);

    if (task_handle_ != nullptr && !task_suspended_for_ota_) {
      vTaskSuspend(task_handle_);
      task_suspended_for_ota_ = true;
    }
#else
    requested_enabled_ = false;
#endif

    force_safe_outputs_();
    ESP_LOGW(TAG, "OTA started: contactor forced OFF, CP forced to -12 V, EVSE task suspended");
    return;
  }

  if (ota_state == ota::OTA_ERROR || ota_state == ota::OTA_ABORT) {
    // Never resume charging automatically after an interrupted update.
#ifdef USE_ESP32
    portENTER_CRITICAL(&data_mux_);
    requested_enabled_ = false;
    requested_available_ = true;
    portEXIT_CRITICAL(&data_mux_);
#else
    requested_enabled_ = false;
    requested_available_ = true;
#endif

    ota_lockout_.store(false, std::memory_order_release);

#ifdef USE_ESP32
    if (task_handle_ != nullptr && task_suspended_for_ota_) {
      task_suspended_for_ota_ = false;
      vTaskResume(task_handle_);
    }
#endif

    ESP_LOGW(TAG, "OTA aborted/failed: EVSE task resumed with Enable forced OFF");
    return;
  }

  // OTA_COMPLETED: remain safe and suspended until the imminent reboot.
}
#endif

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

  // Only used when timing_debug_ is enabled. Unsigned now_us32_() arithmetic is
  // intentionally used so the normal ~71 minute now_us32_() wrap is harmless.
  uint32_t expected_start_us = timing_debug_ ? now_us32_() : 0;
  bool first_cycle = true;

  for (;;) {
    uint32_t cycle_start_us = 0;
    uint32_t lateness_us = 0;

    if (timing_debug_) {
      cycle_start_us = now_us32_();

      if (!first_cycle) {
        const int32_t delta_us = static_cast<int32_t>(cycle_start_us - expected_start_us);
        if (delta_us > 0)
          lateness_us = static_cast<uint32_t>(delta_us);
      }
    }

    first_cycle = false;
    const uint32_t now = now_ms_();

    process_requests_();
    sample_cp_();
    update_adc_supervision_(now);
    update_stable_cp_(sampled_cp_, now);
    update_diode_supervision_(now);
    control_(now);

    if (timing_debug_) {
      const uint32_t runtime_us = static_cast<uint32_t>(now_us32_() - cycle_start_us);

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


uint32_t EVSEComponent::now_ms_() {
#ifdef USE_ESP32
  return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
#else
  return 0;
#endif
}

uint32_t EVSEComponent::now_us32_() {
#ifdef USE_ESP32
  return static_cast<uint32_t>(esp_timer_get_time());
#else
  return 0;
#endif
}

#ifdef USE_ESP32
bool EVSEComponent::setup_pilot_pwm_() {
  ledc_timer_config_t timer_cfg = {};
  timer_cfg.speed_mode = PILOT_LEDC_MODE;
  timer_cfg.duty_resolution = PILOT_LEDC_RESOLUTION;
  timer_cfg.timer_num = PILOT_LEDC_TIMER;
  timer_cfg.freq_hz = PILOT_PWM_HZ;
  timer_cfg.clk_cfg = LEDC_AUTO_CLK;

  esp_err_t err = ledc_timer_config(&timer_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(err));
    return false;
  }

  ledc_channel_config_t channel_cfg = {};
  channel_cfg.gpio_num = pilot_pwm_gpio_num_;
  channel_cfg.speed_mode = PILOT_LEDC_MODE;
  channel_cfg.channel = PILOT_LEDC_CHANNEL;
  channel_cfg.intr_type = LEDC_INTR_DISABLE;
  channel_cfg.timer_sel = PILOT_LEDC_TIMER;
  channel_cfg.duty = 0;
  channel_cfg.hpoint = 0;

  err = ledc_channel_config(&channel_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ledc_channel_config failed: %s", esp_err_to_name(err));
    return false;
  }

  // State A at boot is a true static HIGH, not a 99.9% LEDC waveform.
  err = ledc_stop(PILOT_LEDC_MODE, PILOT_LEDC_CHANNEL, 1);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ledc_stop(HIGH) failed: %s", esp_err_to_name(err));
    return false;
  }

  pilot_pwm_running_ = false;
  pilot_hw_ok_ = true;
  pilot_duty_percent_ = 100.0f;

  const uint32_t actual_hz = ledc_get_freq(PILOT_LEDC_MODE, PILOT_LEDC_TIMER);
  ESP_LOGI(TAG,
           "Native LEDC CP initialized: GPIO%u, requested=%" PRIu32 " Hz actual=%" PRIu32 " Hz",
           static_cast<unsigned int>(pilot_pwm_gpio_num_),
           static_cast<uint32_t>(PILOT_PWM_HZ),
           actual_hz);
  return actual_hz == PILOT_PWM_HZ;
}

void EVSEComponent::shutdown_pilot_pwm_() {
  if (!pilot_hw_ok_)
    return;
  ledc_stop(PILOT_LEDC_MODE, PILOT_LEDC_CHANNEL, 0);
  pilot_pwm_running_ = false;
  pilot_hw_ok_ = false;
}

bool EVSEComponent::set_pilot_static_(bool high) {
  if (!pilot_hw_ok_)
    return false;

  const esp_err_t err = ledc_stop(PILOT_LEDC_MODE, PILOT_LEDC_CHANNEL, high ? 1U : 0U);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ledc_stop(%s) failed: %s", high ? "HIGH" : "LOW", esp_err_to_name(err));
    pilot_hw_ok_ = false;
    return false;
  }

  pilot_pwm_running_ = false;
  pilot_duty_percent_ = high ? 100.0f : 0.0f;
  return true;
}

bool EVSEComponent::start_pilot_pwm_(float duty_fraction) {
  if (!pilot_hw_ok_)
    return false;

  if (duty_fraction < 0.0f)
    duty_fraction = 0.0f;
  if (duty_fraction > 1.0f)
    duty_fraction = 1.0f;

  uint32_t duty_counts = static_cast<uint32_t>(duty_fraction * static_cast<float>(PILOT_LEDC_COUNTS) + 0.5f);
  // For actual PWM we never request 0% or 100%. Clamp anyway so the classic
  // ESP32 LEDC never receives 2^resolution, which is not valid at max count.
  if (duty_counts < 1U)
    duty_counts = 1U;
  if (duty_counts >= PILOT_LEDC_COUNTS)
    duty_counts = PILOT_LEDC_COUNTS - 1U;

  esp_err_t err = ESP_OK;
  if (!pilot_pwm_running_) {
    // Reconfigure the channel after ledc_stop(). This explicitly restarts the
    // peripheral and avoids relying on implicit resume semantics.
    ledc_channel_config_t channel_cfg = {};
    channel_cfg.gpio_num = pilot_pwm_gpio_num_;
    channel_cfg.speed_mode = PILOT_LEDC_MODE;
    channel_cfg.channel = PILOT_LEDC_CHANNEL;
    channel_cfg.intr_type = LEDC_INTR_DISABLE;
    channel_cfg.timer_sel = PILOT_LEDC_TIMER;
    channel_cfg.duty = duty_counts;
    channel_cfg.hpoint = 0;
    err = ledc_channel_config(&channel_cfg);

    if (err != ESP_OK) {
      ESP_LOGE(TAG,
               "Failed to start CP PWM duty=%" PRIu32 "/%" PRIu32 ": %s",
               duty_counts, PILOT_LEDC_COUNTS, esp_err_to_name(err));
    }
  } else {
    // This LEDC channel is owned exclusively by the EVSE task. Use the
    // ordinary set/update pair instead of the thread-safe helper, whose
    // implementation can depend on LEDC fade infrastructure.
    err = ledc_set_duty(PILOT_LEDC_MODE, PILOT_LEDC_CHANNEL, duty_counts);
    if (err == ESP_OK)
      err = ledc_update_duty(PILOT_LEDC_MODE, PILOT_LEDC_CHANNEL);

    if (err != ESP_OK) {
      ESP_LOGE(TAG,
               "Failed to update CP PWM duty=%" PRIu32 "/%" PRIu32 ": %s",
               duty_counts, PILOT_LEDC_COUNTS, esp_err_to_name(err));
    }
  }

  if (err != ESP_OK) {
    // Fail closed: if the requested pilot duty cannot be guaranteed, stop CP
    // PWM, open the contactor and let the state machine enter PILOT_OUTPUT fault.
    ledc_stop(PILOT_LEDC_MODE, PILOT_LEDC_CHANNEL, 0);
    pilot_pwm_running_ = false;
    pilot_hw_ok_ = false;
    pilot_duty_percent_ = 0.0f;
    return false;
  }

  pilot_pwm_running_ = true;
  pilot_duty_percent_ =
      100.0f * static_cast<float>(duty_counts) / static_cast<float>(PILOT_LEDC_COUNTS);

  const uint32_t actual_hz = ledc_get_freq(PILOT_LEDC_MODE, PILOT_LEDC_TIMER);
  ESP_LOGD(TAG, "CP PWM active: %" PRIu32 " Hz, duty=%.2f%% (%" PRIu32 "/%" PRIu32 ")",
           actual_hz, pilot_duty_percent_, duty_counts, PILOT_LEDC_COUNTS);
  return actual_hz == PILOT_PWM_HZ;
}
#endif

#ifdef USE_ESP32
bool EVSEComponent::setup_adc_dma_() {
  adc_unit_t unit = ADC_UNIT_1;
  adc_channel_t channel = ADC_CHANNEL_0;
  esp_err_t err = adc_continuous_io_to_channel(pilot_adc_gpio_num_, &unit, &channel);
  if (err != ESP_OK || unit != ADC_UNIT_1) {
    ESP_LOGE(TAG, "GPIO%u is not an ADC1 channel (%s)", pilot_adc_gpio_num_, esp_err_to_name(err));
    return false;
  }
  adc_channel_ = channel;

  adc_continuous_handle_cfg_t handle_cfg = {};
  handle_cfg.max_store_buf_size = 8192;
  handle_cfg.conv_frame_size = 256;
  handle_cfg.flags.flush_pool = 1;
  err = adc_continuous_new_handle(&handle_cfg, &adc_dma_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "adc_continuous_new_handle failed: %s", esp_err_to_name(err));
    adc_dma_handle_ = nullptr;
    return false;
  }

  adc_digi_pattern_config_t pattern = {};
  pattern.atten = ADC_ATTEN_DB_12;
  pattern.channel = static_cast<uint8_t>(adc_channel_) & 0x7;
  pattern.unit = ADC_UNIT_1;
  pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

  adc_continuous_config_t cfg = {};
  cfg.sample_freq_hz = adc_sample_rate_hz_;
  cfg.conv_mode = ADC_CONV_SINGLE_UNIT_1;
  cfg.format = ADC_DIGI_OUTPUT_FORMAT_TYPE1;
  cfg.pattern_num = 1;
  cfg.adc_pattern = &pattern;

  err = adc_continuous_config(adc_dma_handle_, &cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "adc_continuous_config failed: %s", esp_err_to_name(err));
    shutdown_adc_dma_();
    return false;
  }

  adc_cali_line_fitting_config_t cali_cfg = {};
  cali_cfg.unit_id = ADC_UNIT_1;
  cali_cfg.atten = ADC_ATTEN_DB_12;
  cali_cfg.bitwidth = ADC_BITWIDTH_12;
  cali_cfg.default_vref = 0;
  err = adc_cali_create_scheme_line_fitting(&cali_cfg, &adc_cali_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "ADC calibration init failed: %s", esp_err_to_name(err));
    shutdown_adc_dma_();
    return false;
  }

  err = adc_continuous_start(adc_dma_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "adc_continuous_start failed: %s", esp_err_to_name(err));
    shutdown_adc_dma_();
    return false;
  }

  ESP_LOGI(TAG, "ADC DMA: GPIO%u ADC1_CH%u at %" PRIu32 " samples/s", pilot_adc_gpio_num_,
           static_cast<unsigned>(adc_channel_), adc_sample_rate_hz_);
  return true;
}

void EVSEComponent::shutdown_adc_dma_() {
  if (adc_dma_handle_ != nullptr) {
    adc_continuous_stop(adc_dma_handle_);
    adc_continuous_deinit(adc_dma_handle_);
    adc_dma_handle_ = nullptr;
  }
  if (adc_cali_handle_ != nullptr) {
    adc_cali_delete_scheme_line_fitting(adc_cali_handle_);
    adc_cali_handle_ = nullptr;
  }
}

void EVSEComponent::flush_adc_dma_() {
  if (adc_dma_handle_ == nullptr)
    return;

  // Discard samples acquired under the previous pilot mode so a +12/PWM/-12
  // transition cannot contaminate the next CP decision window.
  for (uint8_t pass = 0; pass < 8; pass++) {
    uint32_t bytes_read = 0;
    const esp_err_t err = adc_continuous_read(adc_dma_handle_, adc_read_buffer_,
                                               sizeof(adc_read_buffer_), &bytes_read, 0);
    if (err == ESP_ERR_TIMEOUT)
      break;
    if (err != ESP_OK) {
      adc_read_errors_++;
      break;
    }
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
    clear_fault_(now_ms_());
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
#ifdef USE_ESP32
  static constexpr uint8_t MAX_PEAK_SAMPLES = 32;
  uint16_t top[MAX_PEAK_SAMPLES] = {};
  uint16_t bottom[MAX_PEAK_SAMPLES];
  for (uint8_t i = 0; i < MAX_PEAK_SAMPLES; i++)
    bottom[i] = UINT16_MAX;

  uint32_t sample_count = 0;
  uint8_t top_count = 0;
  uint8_t bottom_count = 0;
  bool read_error = false;

  // Drain the DMA pool accumulated since the previous 20 ms control cycle.
  // At 80 kS/s this normally yields ~1600 samples / 20 PWM periods. The pool
  // is configured to flush old data on overflow so a delayed task always
  // evaluates the newest CP waveform rather than stale samples.
  for (uint8_t read_pass = 0; read_pass < 8; read_pass++) {
    uint32_t bytes_read = 0;
    const esp_err_t err = adc_continuous_read(adc_dma_handle_, adc_read_buffer_,
                                               sizeof(adc_read_buffer_), &bytes_read, 0);
    if (err == ESP_ERR_TIMEOUT)
      break;
    if (err != ESP_OK) {
      adc_read_errors_++;
      read_error = true;
      break;
    }

    for (uint32_t i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= bytes_read; i += SOC_ADC_DIGI_RESULT_BYTES) {
      const auto *p = reinterpret_cast<const adc_digi_output_data_t *>(&adc_read_buffer_[i]);
      const uint8_t channel = p->type1.channel;
      const uint16_t raw = p->type1.data;
      if (channel != static_cast<uint8_t>(adc_channel_))
        continue;

      sample_count++;

      // Keep the N largest raw readings. Averaging several plateau samples
      // rejects single-sample spikes while still preserving the 100 us
      // positive pulse at the IEC minimum 10% duty cycle.
      uint8_t limit = adc_peak_samples_ < MAX_PEAK_SAMPLES ? adc_peak_samples_ : MAX_PEAK_SAMPLES;
      for (uint8_t pos = 0; pos < limit; pos++) {
        if (pos >= top_count || raw > top[pos]) {
          uint8_t shift_end = top_count < limit ? top_count : static_cast<uint8_t>(limit - 1);
          for (uint8_t j = shift_end; j > pos; j--)
            top[j] = top[j - 1];
          top[pos] = raw;
          if (top_count < limit) top_count++;
          break;
        }
      }

      // Keep the N smallest readings for the negative half-cycle / diode test.
      for (uint8_t pos = 0; pos < limit; pos++) {
        if (pos >= bottom_count || raw < bottom[pos]) {
          uint8_t shift_end = bottom_count < limit ? bottom_count : static_cast<uint8_t>(limit - 1);
          for (uint8_t j = shift_end; j > pos; j--)
            bottom[j] = bottom[j - 1];
          bottom[pos] = raw;
          if (bottom_count < limit) bottom_count++;
          break;
        }
      }
    }
  }

  adc_last_sample_count_ = sample_count;
  if (read_error || sample_count < adc_min_samples_ || top_count == 0 || bottom_count == 0) {
    adc_sample_valid_ = false;
    sampled_cp_ = CpLevel::UNKNOWN;
    diode_sample_valid_ = false;
    return;
  }

  uint32_t top_sum = 0;
  uint32_t bottom_sum = 0;
  for (uint8_t i = 0; i < top_count; i++) top_sum += top[i];
  for (uint8_t i = 0; i < bottom_count; i++) bottom_sum += bottom[i];
  const int high_raw = static_cast<int>((top_sum + top_count / 2U) / top_count);
  const int low_raw = static_cast<int>((bottom_sum + bottom_count / 2U) / bottom_count);

  int high_mv = 0;
  int low_mv = 0;
  if (adc_cali_raw_to_voltage(adc_cali_handle_, high_raw, &high_mv) != ESP_OK ||
      adc_cali_raw_to_voltage(adc_cali_handle_, low_raw, &low_mv) != ESP_OK) {
    adc_read_errors_++;
    adc_sample_valid_ = false;
    sampled_cp_ = CpLevel::UNKNOWN;
    diode_sample_valid_ = false;
    return;
  }

  if (high_mv < 0) high_mv = 0;
  if (low_mv < 0) low_mv = 0;
  cp_high_mv_ = static_cast<uint16_t>(high_mv > 65535 ? 65535 : high_mv);
  cp_low_mv_ = static_cast<uint16_t>(low_mv > 65535 ? 65535 : low_mv);
  adc_sample_valid_ = true;
  sampled_cp_ = classify_cp_(cp_high_mv_);
  diode_sample_valid_ =
      pilot_mode_ != PilotMode::PWM || mv_in_window_(cp_low_mv_, diode_min_mv_, diode_max_mv_);
#endif
}

void EVSEComponent::update_adc_supervision_(uint32_t now) {
  if (adc_sample_valid_) {
    adc_invalid_since_ms_ = 0;
    return;
  }

  if (adc_invalid_since_ms_ == 0) {
    adc_invalid_since_ms_ = now;
    return;
  }

  if ((uint32_t) (now - adc_invalid_since_ms_) >= adc_fault_time_ms_)
    raise_fault_(FaultCode::ADC_FAULT);
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
  // ADC acquisition faults are supervised separately. Do not allow an empty
  // DMA window to become a physical CP state candidate.
  if (sampled == CpLevel::UNKNOWN) {
    candidate_cp_ = CpLevel::UNKNOWN;
    candidate_windows_ = 0;
    candidate_since_ms_ = now;
    invalid_since_ms_ = 0;
    return;
  }

  // A/B/C/D transitions physically cross intermediate voltages. Treat those
  // INVALID samples as a short transition gap instead of promoting INVALID to
  // a stable CP state. The last valid stable state is retained during the
  // grace interval. A continuously invalid voltage still fails closed.
  if (sampled == CpLevel::INVALID) {
    candidate_cp_ = CpLevel::UNKNOWN;
    candidate_windows_ = 0;
    candidate_since_ms_ = now;

    if (invalid_since_ms_ == 0) {
      invalid_since_ms_ = now;
      ESP_LOGD(TAG, "CP entered invalid transition gap (high=%u mV low=%u mV)",
               cp_high_mv_, cp_low_mv_);
      return;
    }

    if ((uint32_t) (now - invalid_since_ms_) >= invalid_grace_time_ms_) {
      ESP_LOGW(TAG,
               "CP remained invalid for %" PRIu32 " ms (high=%u mV low=%u mV)",
               (uint32_t) (now - invalid_since_ms_), cp_high_mv_, cp_low_mv_);
      raise_fault_(FaultCode::PILOT_VOLTAGE);
    }
    return;
  }

  // Any valid A/B/C/D sample immediately clears a transient invalid gap.
  if (invalid_since_ms_ != 0) {
    ESP_LOGD(TAG, "CP left invalid transition gap after %" PRIu32 " ms -> %s",
             (uint32_t) (now - invalid_since_ms_), cp_level_to_string_(sampled));
    invalid_since_ms_ = 0;
  }

  if (sampled != candidate_cp_) {
    candidate_cp_ = sampled;
    candidate_since_ms_ = now;
    candidate_windows_ = 1;
    return;
  }

  if (candidate_windows_ < 255)
    candidate_windows_++;

  if (sampled != stable_cp_ && candidate_windows_ >= cp_confirm_windows_ &&
      (uint32_t) (now - candidate_since_ms_) >= stable_time_ms_) {
    stable_cp_ = sampled;
    ESP_LOGI(TAG, "Stable CP -> %s after %u windows (high=%u mV low=%u mV)",
             cp_level_to_string_(stable_cp_), candidate_windows_, cp_high_mv_, cp_low_mv_);
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
  if (ota_lockout_.load(std::memory_order_acquire)) {
    force_safe_outputs_();
    return;
  }

  if (!pilot_hw_ok_ && fault_code_ == FaultCode::NONE) {
    raise_fault_(FaultCode::PILOT_OUTPUT);
    return;
  }

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
    candidate_windows_ = 0;
    enter_state_(EvseState::A, now);
    return;
  }

  if (stable_cp_ == CpLevel::UNKNOWN) {
    service_state_actions_(now);
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
#ifdef USE_ESP32
  flush_adc_dma_();
#endif

  const char *name = mode == PilotMode::POSITIVE_DC ? "+12 V DC" :
                     mode == PilotMode::PWM ? "1 kHz PWM" : "-12 V DC";
  ESP_LOGI(TAG, "Pilot mode -> %s", name);
}

bool EVSEComponent::apply_pilot_output_() {
#ifdef USE_ESP32
  bool ok = false;

  switch (pilot_mode_) {
    case PilotMode::POSITIVE_DC:
      ok = set_pilot_static_(true);
      break;
    case PilotMode::PWM:
      ok = start_pilot_pwm_(duty_for_current_(current_limit_));
      break;
    case PilotMode::NEGATIVE_DC:
      ok = set_pilot_static_(false);
      break;
  }

  if (!ok) {
    ESP_LOGE(TAG, "CP pilot hardware update failed; contactor forced OFF");
    if (contactor_pin_ != nullptr)
      contactor_pin_->digital_write(false);
    contactor_on_ = false;
  }

  return ok;
#else
  return false;
#endif
}

void EVSEComponent::force_safe_outputs_() {
  if (contactor_pin_ != nullptr)
    contactor_pin_->digital_write(false);

  contactor_on_ = false;
  graceful_stop_active_ = false;
  advertised_current_ = 0.0f;

#ifdef USE_ESP32
  if (pilot_hw_ok_)
    set_pilot_static_(false);  // external CP driver -> -12 V
#endif

  pilot_mode_ = PilotMode::NEGATIVE_DC;
  pilot_duty_percent_ = 0.0f;
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
  if (ota_lockout_.load(std::memory_order_acquire)) {
    ESP_LOGW(TAG, "Contactor close blocked by OTA safety interlock");
    if (contactor_pin_ != nullptr)
      contactor_pin_->digital_write(false);
    contactor_on_ = false;
    return;
  }

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
  fault_since_ms_ = now_ms_();
  ESP_LOGE(TAG, "FAULT: %s", fault_to_string_(code));
  enter_state_(EvseState::E, fault_since_ms_);
}

void EVSEComponent::clear_fault_(uint32_t now) {
  fault_code_ = FaultCode::NONE;
  diode_invalid_since_ms_ = 0;
  diode_valid_ = true;
  stable_cp_ = CpLevel::UNKNOWN;
  candidate_cp_ = CpLevel::UNKNOWN;
  candidate_windows_ = 0;
  invalid_since_ms_ = 0;
  adc_invalid_since_ms_ = 0;

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
    case FaultCode::ADC_FAULT: return "CP ADC/DMA acquisition failed";
    case FaultCode::PILOT_OUTPUT: return "CP PWM/LEDC output failure";
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
  snapshot_adc_sample_count_ = adc_last_sample_count_;
  snapshot_adc_read_errors_ = adc_read_errors_;
  snapshot_pilot_duty_percent_ = pilot_duty_percent_;
  snapshot_pilot_mode_ = pilot_mode_;
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
  uint32_t adc_sample_count;
  uint32_t adc_read_errors;
  float pilot_duty_percent;
  PilotMode pilot_mode;

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
  adc_sample_count = snapshot_adc_sample_count_;
  adc_read_errors = snapshot_adc_read_errors_;
  pilot_duty_percent = snapshot_pilot_duty_percent_;
  pilot_mode = snapshot_pilot_mode_;
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
  if (adc_sample_count_sensor_ != nullptr)
    adc_sample_count_sensor_->publish_state(adc_sample_count);
  if (adc_read_errors_sensor_ != nullptr)
    adc_read_errors_sensor_->publish_state(adc_read_errors);
  if (pilot_duty_sensor_ != nullptr)
    pilot_duty_sensor_->publish_state(pilot_duty_percent);
  if (pilot_mode_sensor_ != nullptr) {
    const char *mode_name =
        pilot_mode == PilotMode::POSITIVE_DC ? "+12 V DC" :
        pilot_mode == PilotMode::PWM ? "1 kHz PWM" : "-12 V DC";
    pilot_mode_sensor_->publish_state(mode_name);
  }

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
    const uint32_t now = now_ms_();
    const uint32_t timeout_ms = sample_interval_ms_ * 10U < 500U ? 500U : sample_interval_ms_ * 10U;
    const bool task_running = heartbeat_ms != 0 && (uint32_t) (now - heartbeat_ms) <= timeout_ms;
    task_running_sensor_->publish_state(task_running);
  }
}

}  // namespace evse
}  // namespace esphome

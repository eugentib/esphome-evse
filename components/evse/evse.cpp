#include "evse.h"
#ifdef USE_ARDUINO
#include <Arduino.h>
#endif

namespace esphome { namespace evse {
static const char *const TAG="evse";

void EVSEComponent::setup(){
#ifdef USE_ARDUINO
  pinMode(pilot_adc_pin_, INPUT);
  analogReadResolution(12);
#else
  ESP_LOGE(TAG,"This MVP currently requires Arduino framework");
  mark_failed(); return;
#endif
  enabled_=false; pwm_active_=false; contactor_on_=false; fault_=false;
  open_contactor_(); apply_pilot_output_();
  candidate_since_ms_=stable_since_ms_=millis();
  ESP_LOGI(TAG,"EVSE initialized; charging disabled after boot");
}

void EVSEComponent::dump_config(){
  ESP_LOGCONFIG(TAG,"ESPHome EVSE");
  ESP_LOGCONFIG(TAG,"  Pilot ADC GPIO: %d",pilot_adc_pin_);
  ESP_LOGCONFIG(TAG,"  Max current: %.1f A",max_current_);
  ESP_LOGCONFIG(TAG,"  Default current: %.1f A",default_current_);
}

void EVSEComponent::loop(){
  const uint32_t now=millis();
  if((uint32_t)(now-last_sample_ms_)>=sample_interval_ms_){
    last_sample_ms_=now; sample_cp_(); update_stable_state_(sampled_state_); control_();
  }
  if((uint32_t)(now-last_publish_ms_)>=500){ last_publish_ms_=now; publish_(); }
}

void EVSEComponent::set_enabled(bool enabled){
  enabled_=enabled;
  ESP_LOGI(TAG,"EVSE enable=%s",YESNO(enabled));
  if(!enabled) open_contactor_();
}

void EVSEComponent::set_current_limit(float amps){
  if(amps<6.0f) amps=6.0f;
  if(amps>max_current_) amps=max_current_;
  current_limit_=amps;
  if(pwm_active_) apply_pilot_output_();
}

float EVSEComponent::duty_for_current_(float amps) const{
  float pct=amps/0.6f;
  if(pct<10.0f)pct=10.0f;
  if(pct>85.0f)pct=85.0f;
  return pct/100.0f;
}

void EVSEComponent::set_pwm_active_(bool active){
  if(pwm_active_==active)return;
  pwm_active_=active; apply_pilot_output_();
}

void EVSEComponent::apply_pilot_output_(){
  if(!pilot_output_)return;
  pilot_output_->set_level(pwm_active_ ? duty_for_current_(current_limit_) : 1.0f);
}

void EVSEComponent::sample_cp_(){
#ifdef USE_ARDUINO
  uint16_t lo=4095,hi=0;
  const uint32_t start=micros();
  while((uint32_t)(micros()-start)<sample_window_us_){
    int v=analogRead(pilot_adc_pin_);
    if(v<lo)lo=v;
    if(v>hi)hi=v;
  }
  cp_high_raw_=hi; cp_low_raw_=lo;
  sampled_state_=classify_state_(hi);
  diode_ok_=!pwm_active_ || (lo<=diode_max_raw_);
#endif
}

PilotState EVSEComponent::classify_state_(uint16_t v) const{
  if(v>=state_a_min_raw_)return PilotState::A;
  if(v>=state_b_min_raw_)return PilotState::B;
  if(v>=state_c_min_raw_)return PilotState::C;
  if(v>=state_d_min_raw_)return PilotState::D;
  return PilotState::E;
}

void EVSEComponent::update_stable_state_(PilotState s){
  const uint32_t now=millis();
  if(s!=candidate_state_){candidate_state_=s; candidate_since_ms_=now; return;}
  if(s!=stable_state_ && (uint32_t)(now-candidate_since_ms_)>=stable_time_ms_){
    stable_state_=s; stable_since_ms_=now;
    ESP_LOGI(TAG,"CP -> %s high=%u low=%u diode=%s",state_to_string_(s),cp_high_raw_,cp_low_raw_,YESNO(diode_ok_));
  }
}

void EVSEComponent::control_(){
  const uint32_t now=millis();
  switch(stable_state_){
    case PilotState::A:
      open_contactor_(); set_fault_(false); set_pwm_active_(false); break;
    case PilotState::B:
      open_contactor_(); set_fault_(false); set_pwm_active_(enabled_);
      if(enabled_ && pwm_active_ && !diode_ok_) set_fault_(true,"CP diode check failed");
      break;
    case PilotState::C:
      if(!enabled_){open_contactor_(); set_pwm_active_(false); set_fault_(false); break;}
      set_pwm_active_(true);
      if(!diode_ok_){open_contactor_(); set_fault_(true,"CP diode check failed"); break;}
      set_fault_(false);
      if(!contactor_on_ && (uint32_t)(now-stable_since_ms_)>=contactor_close_delay_ms_) close_contactor_();
      break;
    case PilotState::D:
      open_contactor_(); set_pwm_active_(false); set_fault_(true,"State D / ventilation requested"); break;
    default:
      open_contactor_(); set_pwm_active_(false); set_fault_(true,"Invalid CP state"); break;
  }
}

void EVSEComponent::open_contactor_(){
  if(contactor_)contactor_->turn_off();
  if(contactor_on_){ESP_LOGI(TAG,"Contactor OFF"); contactor_on_=false;}
}

void EVSEComponent::close_contactor_(){
  if(fault_ || !enabled_ || stable_state_!=PilotState::C)return;
  if(contactor_)contactor_->turn_on();
  if(!contactor_on_){ESP_LOGI(TAG,"Contactor ON"); contactor_on_=true;}
}

void EVSEComponent::set_fault_(bool f,const char *reason){
  fault_=f;
  if(f){
    fault_reason_=reason?reason:"fault";
    ESP_LOGW(TAG,"FAULT: %s",fault_reason_.c_str());
    open_contactor_();
  } else fault_reason_.clear();
}

const char *EVSEComponent::state_to_string_(PilotState s) const{
  switch(s){
    case PilotState::A:return "A - disconnected";
    case PilotState::B:return "B - connected";
    case PilotState::C:return "C - charging requested";
    case PilotState::D:return "D - ventilation";
    case PilotState::E:return "E - fault";
    default:return "Unknown";
  }
}

void EVSEComponent::publish_(){
  if(state_sensor_)state_sensor_->publish_state(state_to_string_(stable_state_));
  if(cp_high_raw_sensor_)cp_high_raw_sensor_->publish_state(cp_high_raw_);
  if(cp_low_raw_sensor_)cp_low_raw_sensor_->publish_state(cp_low_raw_);
  if(advertised_current_sensor_)advertised_current_sensor_->publish_state(pwm_active_?current_limit_:0.0f);
  if(vehicle_connected_sensor_){
    bool c=stable_state_==PilotState::B||stable_state_==PilotState::C||stable_state_==PilotState::D;
    vehicle_connected_sensor_->publish_state(c);
  }
  if(charging_sensor_)charging_sensor_->publish_state(contactor_on_);
  if(fault_sensor_)fault_sensor_->publish_state(fault_);
}

}} // namespace esphome::evse

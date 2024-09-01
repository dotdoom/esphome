#pragma once

#include "BleScanner.h"
#include "NukiConstants.h"
#include "NukiLock.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/button/button.h"
#include "esphome/components/lock/lock.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/component.h"
#include "esphome/core/time.h"

namespace esphome {
namespace nuki_lock {

class NukiLockComponent : public lock::Lock,
                          public PollingComponent,
                          public Nuki::SmartlockEventHandler {
 public:
  explicit NukiLockComponent()
      : Lock(),
        last_passive_update_millis_(0),
        restart_after_beacon_latency_(0),
        update_scheduled_(false),
        request_battery_reports_(false),
        pin_set_(false){};

  // Component.
  void setup() override;
  float get_setup_priority() const override {
    return setup_priority::HARDWARE - 1.0f;
  }
  void dump_config() override;

  // PollingComponent.
  void update() override;

  // Nuki::SmartlockEventHandler.
  void notify(Nuki::EventType event);

  // Passive updates (predefined update frequency).
  SUB_BINARY_SENSOR(paired)
  SUB_SENSOR(beacon_rssi)
  SUB_SENSOR(beacon_latency)
  SUB_SENSOR(heartbeat_latency)
  SUB_TEXT_SENSOR(beacon_ble_address)

  // Active updates (configured update_interval frequency).
  // Key turner state.
  SUB_BINARY_SENSOR(battery_critical)
  SUB_SENSOR(battery_level)
  SUB_BINARY_SENSOR(door_contact)
  SUB_TEXT_SENSOR(door_contact_sensor)
  SUB_TEXT_SENSOR(lock_current_datetime)
  SUB_TEXT_SENSOR(error)
  SUB_TEXT_SENSOR(trigger)
  SUB_SENSOR(config_update_count)
  SUB_TEXT_SENSOR(last_action)
  SUB_TEXT_SENSOR(last_action_trigger)
  SUB_TEXT_SENSOR(last_action_completion_status)
  SUB_BINARY_SENSOR(night_mode_active)
  // Battery report.
  SUB_SENSOR(battery_voltage)
  SUB_SENSOR(battery_resistance)
  SUB_SENSOR(last_action_lowest_voltage)
  SUB_SENSOR(last_action_start_voltage)
  SUB_SENSOR(last_action_lock_distance)
  SUB_SENSOR(last_action_start_temperature)
  SUB_SENSOR(last_action_battery_drain)
  SUB_SENSOR(last_action_max_turn_current)

  // Actions.
  SUB_BUTTON(lock_n_go)
  SUB_BUTTON(unpair)
  SUB_BUTTON(request_state)

  void set_request_battery_reports(bool value) {
    request_battery_reports_ = value;
  }

  void set_restart_after_beacon_latency(unsigned long value) {
    restart_after_beacon_latency_ = value;
  }

  void set_pin(unsigned long value) {
    pin_ = value;
    pin_set_ = true;
  }

  void set_time_source(time::RealTimeClock *value) { time_source_ = value; }

  // RequestStateButton.
  void schedule_update();

  // UnpairButton.
  void unpair();

 protected:
  // lock::Lock.
  void control(const lock::LockCall &call) override;
  void open_latch() override;

 private:
  bool update_key_turner_state_();
  bool update_battery_report_();
  bool update_lock_time_();
  void ble_loop_();
  void send_action_(NukiLock::LockAction action);

  NukiLock::NukiLock *nuki_lock_;
  BleScanner::Scanner scanner_;
  time::RealTimeClock *time_source_{nullptr};
  uint16_t pin_;
  bool pin_set_;

  unsigned long last_passive_update_millis_;
  unsigned long restart_after_beacon_latency_;
  bool update_scheduled_;
  bool request_battery_reports_;

  uint32_t our_device_id_;
  std::string our_device_name_;
};

class RequestStateButton : public button::Button,
                           public Parented<NukiLockComponent> {
 public:
  RequestStateButton() = default;

 protected:
  void press_action() override { get_parent()->schedule_update(); }
};

class UnpairButton : public button::Button, public Parented<NukiLockComponent> {
 public:
  UnpairButton() = default;

 protected:
  void press_action() override { get_parent()->unpair(); }
};

}  // namespace nuki_lock
}  // namespace esphome

#include "nuki_lock.h"

#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace {

using namespace esphome;

const char *TAG = "nuki_lock";

class Adapt {
 public:
  static esphome::lock::LockState lock_state_to_esphome(
      NukiLock::LockState state) {
    switch (state) {
      case NukiLock::LockState::Locked:
        return esphome::lock::LOCK_STATE_LOCKED;
      case NukiLock::LockState::Unlocked:
        return esphome::lock::LOCK_STATE_UNLOCKED;
      case NukiLock::LockState::MotorBlocked:
        return esphome::lock::LOCK_STATE_JAMMED;
      case NukiLock::LockState::Locking:
        return esphome::lock::LOCK_STATE_LOCKING;
      case NukiLock::LockState::Unlocking:
        return esphome::lock::LOCK_STATE_UNLOCKING;
      default:
        return esphome::lock::LOCK_STATE_NONE;
    }
  }

  static std::string lock_action_to_string(NukiLock::LockAction action) {
    char result[256];
    NukiLock::lockactionToString(action, result);
    return result;
  }

  static bool door_sensor_state_to_is_door_open(Nuki::DoorSensorState state) {
    switch (state) {
      case Nuki::DoorSensorState::DoorClosed:
        return false;
      default:
        return true;
    }
  }

  static std::string door_sensor_state_to_string(Nuki::DoorSensorState state) {
    char result[256];
    NukiLock::doorSensorStateToString(state, result);
    return result;
  }

  static std::string key_turner_datetime_iso(
      const NukiLock::KeyTurnerState &state) {
    uint8_t tzOffsetHours = abs(state.timeZoneOffset / 60);
    uint8_t tzOffsetMinutes = abs(state.timeZoneOffset % 60);

    char ts[256];
    sprintf(ts, "%d-%.2d-%.2dT%.2d:%.2d:%.2d%c%.2d:%.2d", state.currentTimeYear,
            state.currentTimeMonth, state.currentTimeDay, state.currentTimeHour,
            state.currentTimeMinute, state.currentTimeSecond,
            state.timeZoneOffset >= 0 ? '+' : '-', tzOffsetHours,
            tzOffsetMinutes);
    return ts;
  }

  static std::string trigger_to_string(NukiLock::Trigger trigger) {
    char result[256];
    NukiLock::triggerToString(trigger, result);
    return result;
  }

  static std::string completion_status_to_string(
      NukiLock::CompletionStatus status) {
    char result[256];
    NukiLock::completionStatusToString(status, result);
    return result;
  }

  static std::string cmd_result_to_string(Nuki::CmdResult result) {
    char resultStr[256];
    NukiLock::cmdResultToString(result, resultStr);
    return resultStr;
  }

  static std::string error_to_string(NukiLock::ErrorCode error) {
    switch (error) {
      case NukiLock::ErrorCode::ERROR_BAD_CRC:
        return "ERROR_BAD_CRC";
      case NukiLock::ErrorCode::ERROR_BAD_LENGTH:
        return "ERROR_BAD_LENGTH";
      case NukiLock::ErrorCode::ERROR_UNKNOWN:
        return "ERROR_UNKNOWN";
      case NukiLock::ErrorCode::P_ERROR_NOT_PAIRING:
        return "P_ERROR_NOT_PAIRING";
      case NukiLock::ErrorCode::P_ERROR_BAD_AUTHENTICATOR:
        return "P_ERROR_BAD_AUTHENTICATOR";
      case NukiLock::ErrorCode::P_ERROR_BAD_PARAMETER:
        return "P_ERROR_BAD_PARAMETER";
      case NukiLock::ErrorCode::P_ERROR_MAX_USER:
        return "P_ERROR_MAX_USER";
      case NukiLock::ErrorCode::K_ERROR_NOT_AUTHORIZED:
        return "K_ERROR_NOT_AUTHORIZED";
      case NukiLock::ErrorCode::K_ERROR_BAD_PIN:
        return "K_ERROR_BAD_PIN";
      case NukiLock::ErrorCode::K_ERROR_BAD_NONCE:
        return "K_ERROR_BAD_NONCE";
      case NukiLock::ErrorCode::K_ERROR_BAD_PARAMETER:
        return "K_ERROR_BAD_PARAMETER";
      case NukiLock::ErrorCode::K_ERROR_INVALID_AUTH_ID:
        return "K_ERROR_INVALID_AUTH_ID";
      case NukiLock::ErrorCode::K_ERROR_DISABLED:
        return "K_ERROR_DISABLED";
      case NukiLock::ErrorCode::K_ERROR_REMOTE_NOT_ALLOWED:
        return "K_ERROR_REMOTE_NOT_ALLOWED";
      case NukiLock::ErrorCode::K_ERROR_TIME_NOT_ALLOWED:
        return "K_ERROR_TIME_NOT_ALLOWED";
      case NukiLock::ErrorCode::K_ERROR_TOO_MANY_PIN_ATTEMPTS:
        return "K_ERROR_TOO_MANY_PIN_ATTEMPTS";
      case NukiLock::ErrorCode::K_ERROR_TOO_MANY_ENTRIES:
        return "K_ERROR_TOO_MANY_ENTRIES";
      case NukiLock::ErrorCode::K_ERROR_CODE_ALREADY_EXISTS:
        return "K_ERROR_CODE_ALREADY_EXISTS";
      case NukiLock::ErrorCode::K_ERROR_CODE_INVALID:
        return "K_ERROR_CODE_INVALID";
      case NukiLock::ErrorCode::K_ERROR_CODE_INVALID_TIMEOUT_1:
        return "K_ERROR_CODE_INVALID_TIMEOUT_1";
      case NukiLock::ErrorCode::K_ERROR_CODE_INVALID_TIMEOUT_2:
        return "K_ERROR_CODE_INVALID_TIMEOUT_2";
      case NukiLock::ErrorCode::K_ERROR_CODE_INVALID_TIMEOUT_3:
        return "K_ERROR_CODE_INVALID_TIMEOUT_3";
      case NukiLock::ErrorCode::K_ERROR_AUTO_UNLOCK_TOO_RECENT:
        return "K_ERROR_AUTO_UNLOCK_TOO_RECENT";
      case NukiLock::ErrorCode::K_ERROR_POSITION_UNKNOWN:
        return "K_ERROR_POSITION_UNKNOWN";
      case NukiLock::ErrorCode::K_ERROR_MOTOR_BLOCKED:
        return "K_ERROR_MOTOR_BLOCKED";
      case NukiLock::ErrorCode::K_ERROR_CLUTCH_FAILURE:
        return "K_ERROR_CLUTCH_FAILURE";
      case NukiLock::ErrorCode::K_ERROR_MOTOR_TIMEOUT:
        return "K_ERROR_MOTOR_TIMEOUT";
      case NukiLock::ErrorCode::K_ERROR_BUSY:
        return "K_ERROR_BUSY";
      case NukiLock::ErrorCode::K_ERROR_CANCELED:
        return "K_ERROR_CANCELED";
      case NukiLock::ErrorCode::K_ERROR_NOT_CALIBRATED:
        return "K_ERROR_NOT_CALIBRATED";
      case NukiLock::ErrorCode::K_ERROR_MOTOR_POSITION_LIMIT:
        return "K_ERROR_MOTOR_POSITION_LIMIT";
      case NukiLock::ErrorCode::K_ERROR_MOTOR_LOW_VOLTAGE:
        return "K_ERROR_MOTOR_LOW_VOLTAGE";
      case NukiLock::ErrorCode::K_ERROR_MOTOR_POWER_FAILURE:
        return "K_ERROR_MOTOR_POWER_FAILURE";
      case NukiLock::ErrorCode::K_ERROR_CLUTCH_POWER_FAILURE:
        return "K_ERROR_CLUTCH_POWER_FAILURE";
      case NukiLock::ErrorCode::K_ERROR_VOLTAGE_TOO_LOW:
        return "K_ERROR_VOLTAGE_TOO_LOW";
      case NukiLock::ErrorCode::K_ERROR_FIRMWARE_UPDATE_NEEDED:
        return "K_ERROR_FIRMWARE_UPDATE_NEEDED";
      default:
        return std::string("Unknown #") +
               std::to_string(static_cast<uint8_t>(error));
    }
  }
};
}  // namespace

namespace esphome {
namespace nuki_lock {

bool NukiLockComponent::update_key_turner_state_() {
  NukiLock::KeyTurnerState retrievedKeyTurnerState;
  Nuki::CmdResult result =
      nuki_lock_->requestKeyTurnerState(&retrievedKeyTurnerState);

  if (result != Nuki::CmdResult::Success) {
    ESP_LOGE(TAG, "request for key turner state failed: %s",
             Adapt::cmd_result_to_string(result).c_str());

    publish_state(lock::LOCK_STATE_NONE);
    error_text_sensor_->publish_state(
        Adapt::error_to_string(nuki_lock_->getLastError()));

    // TODO(https://github.com/esphome/feature-requests/issues/1568): publish
    // unavailable for all related sensors.

    schedule_update();

    return false;
  }

  publish_state(
      Adapt::lock_state_to_esphome(retrievedKeyTurnerState.lockState));

  door_contact_binary_sensor_->publish_state(
      Adapt::door_sensor_state_to_is_door_open(
          retrievedKeyTurnerState.doorSensorState));
  door_contact_sensor_text_sensor_->publish_state(
      Adapt::door_sensor_state_to_string(
          retrievedKeyTurnerState.doorSensorState));
  battery_level_sensor_->publish_state(nuki_lock_->getBatteryPerc());
  battery_critical_binary_sensor_->publish_state(
      nuki_lock_->isBatteryCritical());
  trigger_text_sensor_->publish_state(
      Adapt::trigger_to_string(retrievedKeyTurnerState.trigger));
  night_mode_active_binary_sensor_->publish_state(
      retrievedKeyTurnerState.nightModeActive > 0);

  if (lock_current_datetime_text_sensor_ != nullptr) {
    lock_current_datetime_text_sensor_->publish_state(
        Adapt::key_turner_datetime_iso(retrievedKeyTurnerState));
  }
  if (config_update_count_sensor_ != nullptr) {
    config_update_count_sensor_->publish_state(
        retrievedKeyTurnerState.configUpdateCount);
  }
  if (last_action_text_sensor_ != nullptr) {
    last_action_text_sensor_->publish_state(
        Adapt::lock_action_to_string(retrievedKeyTurnerState.lastLockAction));
  }
  if (last_action_trigger_text_sensor_ != nullptr) {
    last_action_trigger_text_sensor_->publish_state(Adapt::trigger_to_string(
        retrievedKeyTurnerState.lastLockActionTrigger));
  }
  if (last_action_completion_status_text_sensor_ != nullptr) {
    last_action_completion_status_text_sensor_->publish_state(
        Adapt::completion_status_to_string(
            retrievedKeyTurnerState.lastLockActionCompletionStatus));
  }

  return true;
}

bool NukiLockComponent::update_lock_time_() {
  if (time_source_ == nullptr) {
    ESP_LOGE(TAG, "updating lock time failed: time source not set");
    return false;
  }

  Nuki::CmdResult result = nuki_lock_->verifySecurityPin();
  if (result != Nuki::CmdResult::Success) {
    ESP_LOGE(TAG, "verifying security pin for lock time update failed: %s",
             Adapt::cmd_result_to_string(result).c_str());
    error_text_sensor_->publish_state(
        Adapt::error_to_string(nuki_lock_->getLastError()));
    return false;
  }

  ESPTime now = time_source_->utcnow();
  if (!now.is_valid()) {
    ESP_LOGE(TAG, "updating lock time failed: time source is invalid");
    return false;
  }

  Nuki::TimeValue tv = {
      .year = now.year,
      .month = now.month,
      .day = now.day_of_month,
      .hour = now.hour,
      .minute = now.minute,
      .second = now.second,
  };

  ESP_LOGI(TAG, "Setting lock time: %.4d-%.2d-%.2d %.2d:%.2d:%.2d", tv.year,
           tv.month, tv.day, tv.hour, tv.minute, tv.second);
  result = nuki_lock_->updateTime(tv);
  if (result != Nuki::CmdResult::Success) {
    ESP_LOGE(TAG, "updating lock time failed: %s",
             Adapt::cmd_result_to_string(result).c_str());
    error_text_sensor_->publish_state(
        Adapt::error_to_string(nuki_lock_->getLastError()));
    return false;
  }
  return true;
}

bool NukiLockComponent::update_battery_report_() {
  NukiLock::BatteryReport batteryReport;
  Nuki::CmdResult result =
      this->nuki_lock_->requestBatteryReport(&batteryReport);

  if (result != Nuki::CmdResult::Success) {
    ESP_LOGE(TAG, "request for battery report failed: %s",
             Adapt::cmd_result_to_string(result).c_str());
    // TODO(https://github.com/esphome/feature-requests/issues/1568): publish
    // unavailable for all related sensors.
    error_text_sensor_->publish_state(
        Adapt::error_to_string(nuki_lock_->getLastError()));

    return false;
  }

  battery_voltage_sensor_->publish_state(batteryReport.batteryVoltage /
                                         1000.0f);
  battery_resistance_sensor_->publish_state(batteryReport.batteryResistance /
                                            1000.0f);
  last_action_lowest_voltage_sensor_->publish_state(
      batteryReport.lowestVoltage / 1000.0f);
  last_action_start_voltage_sensor_->publish_state(batteryReport.startVoltage /
                                                   1000.0f);
  last_action_lock_distance_sensor_->publish_state(batteryReport.lockDistance);
  last_action_start_temperature_sensor_->publish_state(
      batteryReport.startTemperature);
  // Milliwattseconds (mWs) to Wh.
  last_action_battery_drain_sensor_->publish_state(batteryReport.batteryDrain /
                                                   1000.0f / 3600.0f);
  last_action_max_turn_current_sensor_->publish_state(
      batteryReport.maxTurnCurrent / 1000.0f);

  return true;
}

void NukiLockComponent::notify(Nuki::EventType event) {
  ESP_LOGI(TAG, "received Nuki event - updating state");
  // Have to execute later as this runs in BLE scanner context and we can't do
  // any BLE in this context.
  schedule_update();
}

void NukiLockComponent::schedule_update() {
  if (!update_scheduled_) {
    update_scheduled_ = true;
    defer([this]() {
      update();
      update_scheduled_ = false;
    });
  }
}

void NukiLockComponent::setup() {
  traits.set_supports_open(true);
  traits.set_supported_states(std::set<lock::LockState>{
      lock::LOCK_STATE_NONE, lock::LOCK_STATE_LOCKED, lock::LOCK_STATE_UNLOCKED,
      lock::LOCK_STATE_JAMMED, lock::LOCK_STATE_LOCKING,
      lock::LOCK_STATE_UNLOCKING});
  publish_state(lock::LOCK_STATE_NONE);

  uint8_t mac[6];
  get_mac_address_raw(mac);
  our_device_id_ = mac[0] + (mac[1] << 8) + (mac[2] << 16) + (mac[3] << 24);
  // name will be used as part of the Preferences key to save credentials.
  // There's a limit of 15 characters on key length, keep it predictable.
  our_device_name_ = "nuki" + std::to_string(get_object_id_hash());

  nuki_lock_ = new NukiLock::NukiLock(our_device_name_, our_device_id_);
  scanner_.initialize();
  nuki_lock_->registerBleScanner(&scanner_);
  nuki_lock_->initialize();
  nuki_lock_->setEventHandler(this);

  if (pin_set_) {
    nuki_lock_->saveSecurityPincode(pin_);
  }

  bool paired = nuki_lock_->isPairedWithLock();
  paired_binary_sensor_->publish_initial_state(paired);
  error_text_sensor_->publish_state("No error");

  set_interval("ble_loop", 320 /* ms*/, [this]() { ble_loop_(); });
  schedule_update();
}

void NukiLockComponent::unpair() {
  ESP_LOGI(TAG, "unpairing Nuki lock");
  nuki_lock_->unPairNuki();
  // Force BLE update on the next loop.
  last_passive_update_millis_ = 0;
  // And a regular fetch.
  schedule_update();
}

void NukiLockComponent::ble_loop_() {
  scanner_.update();

  unsigned long ts = millis();
  bool should_publish_update = last_passive_update_millis_ == 0 ||
                               last_passive_update_millis_ + 60000 < ts;

  bool paired = nuki_lock_->isPairedWithLock();
  if (should_publish_update) {
    last_passive_update_millis_ = ts;
    paired_binary_sensor_->publish_state(paired);
  }

  if (!paired) {
    if (nuki_lock_->pairNuki() == Nuki::PairingResult::Success) {
      ESP_LOGI(TAG, "Nuki lock successfully paired!");
      // Force BLE update on the next loop.
      last_passive_update_millis_ = 0;
      // And a regular fetch.
      schedule_update();
    } else if (should_publish_update) {
      ESP_LOGW(TAG,
               "Nuki lock is not yet paired. Make sure Bluetooth pairing "
               "is enabled in lock settings, then press and hold the button "
               "on the lock for a few seconds until LED glows constantly");
    }

    return;
  }

  nuki_lock_->updateConnectionState();
  // Update timestamp as we might have gotten a refresh packet on BLE.
  ts = millis();
  unsigned long beaconLatency = ts - nuki_lock_->getLastReceivedBeaconTs(),
                heartbeatLatency = ts - nuki_lock_->getLastHeartbeat();
  if (should_publish_update) {
    if (beacon_rssi_sensor_ != nullptr) {
      int rssi = nuki_lock_->getRssi();
      if (rssi != 0) {
        beacon_rssi_sensor_->publish_state(rssi);
      }
    }
    if (beacon_latency_sensor_ != nullptr) {
      beacon_latency_sensor_->publish_state(beaconLatency);
    }
    if (heartbeat_latency_sensor_ != nullptr) {
      heartbeat_latency_sensor_->publish_state(heartbeatLatency);
    }
    if (beacon_ble_address_text_sensor_ != nullptr) {
      beacon_ble_address_text_sensor_->publish_state(
          nuki_lock_->getBleAddress().toString());
    }
  }

  if (restart_after_beacon_latency_ != 0 &&
      beaconLatency > restart_after_beacon_latency_) {
    ESP_LOGE(TAG,
             "Beacon latency %d higher than configured maximum %d. Restarting "
             "the device");
    App.safe_reboot();
  }
}

void NukiLockComponent::update() {
  bool paired = nuki_lock_->isPairedWithLock();
  if (paired) {
    if (time_source_ != nullptr) {
      // Update time first, then request the state so we get updated time.
      update_lock_time_();
    }
    update_key_turner_state_();
    if (request_battery_reports_) {
      update_battery_report_();
    }
  }
}

void NukiLockComponent::send_action_(NukiLock::LockAction action) {
  if (!this->nuki_lock_->isPairedWithLock()) {
    ESP_LOGE(TAG, "lock or unlock action %d called before a lock is paired",
             action);
    return;
  }
  Nuki::CmdResult result = this->nuki_lock_->lockAction(action);
  if (result != Nuki::CmdResult::Success) {
    ESP_LOGE(TAG, "setting state %d failed: %s", action,
             Adapt::cmd_result_to_string(result).c_str());
    publish_state(lock::LOCK_STATE_NONE);
    error_text_sensor_->publish_state(
        Adapt::error_to_string(nuki_lock_->getLastError()));
    schedule_update();
  }
}

void NukiLockComponent::control(const lock::LockCall &call) {
  auto action = *call.get_state();
  switch (action) {
    case lock::LOCK_STATE_LOCKED:
      publish_state(lock::LOCK_STATE_LOCKING);
      send_action_(NukiLock::LockAction::Lock);
      break;

    case lock::LOCK_STATE_UNLOCKED:
      publish_state(lock::LOCK_STATE_UNLOCKING);
      send_action_(NukiLock::LockAction::Unlock);
      break;

    default:
      ESP_LOGE(TAG, "unsupported lock state requested: %d", action);
      return;
  }

  // Final lock state will be published once Nuki informs us of an update.
}

void NukiLockComponent::open_latch() {
  publish_state(lock::LOCK_STATE_UNLOCKING);
  send_action_(NukiLock::LockAction::Unlatch);
}

void NukiLockComponent::dump_config() {
  char device_id[256];
  sprintf(device_id, "%s (0x%.8x)", our_device_name_.c_str(), our_device_id_);
  LOG_LOCK("", device_id, this);

  // Required.
  LOG_BINARY_SENSOR("  ", "", paired_binary_sensor_);
  LOG_TEXT_SENSOR("  ", "", error_text_sensor_);
  LOG_BINARY_SENSOR("  ", "", battery_critical_binary_sensor_);
  LOG_SENSOR("  ", "", battery_level_sensor_);
  LOG_BINARY_SENSOR("  ", "", door_contact_binary_sensor_);
  LOG_TEXT_SENSOR("  ", "", door_contact_sensor_text_sensor_);
  LOG_TEXT_SENSOR("  ", "", trigger_text_sensor_);
  LOG_BINARY_SENSOR("  ", "", night_mode_active_binary_sensor_);

  // Optional.
  LOG_SENSOR("  ", "", beacon_rssi_sensor_);
  LOG_SENSOR("  ", "", beacon_latency_sensor_);
  LOG_SENSOR("  ", "", heartbeat_latency_sensor_);
  LOG_TEXT_SENSOR("  ", "", beacon_ble_address_text_sensor_);
  LOG_TEXT_SENSOR("  ", "", lock_current_datetime_text_sensor_);
  LOG_SENSOR("  ", "", config_update_count_sensor_);
  LOG_TEXT_SENSOR("  ", "", last_action_text_sensor_);
  LOG_TEXT_SENSOR("  ", "", last_action_trigger_text_sensor_);
  LOG_TEXT_SENSOR("  ", "", last_action_completion_status_text_sensor_);

  if (request_battery_reports_) {
    // Optional sensors - battery report.
    LOG_SENSOR("  ", "", battery_voltage_sensor_);
    LOG_SENSOR("  ", "", battery_resistance_sensor_);
    LOG_SENSOR("  ", "", last_action_lowest_voltage_sensor_);
    LOG_SENSOR("  ", "", last_action_start_voltage_sensor_);
    LOG_SENSOR("  ", "", last_action_lock_distance_sensor_);
    LOG_SENSOR("  ", "", last_action_start_temperature_sensor_);
    LOG_SENSOR("  ", "", last_action_battery_drain_sensor_);
    LOG_SENSOR("  ", "", last_action_max_turn_current_sensor_);
  }

  ESP_LOGCONFIG(TAG, "  Restart after beacon latency: %dms",
                restart_after_beacon_latency_);
  if (time_source_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Time source: set");
  }
  if (pin_set_) {
    ESP_LOGCONFIG(
        TAG, "  Pin: " ESPHOME_LOG_SECRET_BEGIN "%d" ESPHOME_LOG_SECRET_END,
        pin_);
  }
}

}  // namespace nuki_lock
}  // namespace esphome

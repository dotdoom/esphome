#include "zehnder.h"

#include "esphome/components/remote_base/remote_base.h"

namespace {
static const char* const TAG = __FILE__;
static const uint8_t MIN_TEMPERATURE = 35;
static const uint8_t MAX_TEMPERATURE = 70;
static const uint8_t TEMPERATURE_LEVELS = 8;

static void add_header_(esphome::remote_base::RemoteTransmitData *data) {
  data->item(30, 1000);
}

static void add_byte_(esphome::remote_base::RemoteTransmitData *data, uint8_t value) {
  for (int i = 0; i < 8; ++i) {
    data->item(30, value & 0b10000000 ? 830 : 650);
    value <<= 1;
  }
}

static void add_post_(esphome::remote_base::RemoteTransmitData *data) {
  data->item(30, 460);
  data->item(30, 650);
}
}

namespace esphome {
namespace zehnder {

void ZehnderComponent::control(const climate::ClimateCall &call) {
  float target_temp = this->target_temperature;
  climate::ClimateMode mode = this->mode;

  if (call.get_target_temperature().has_value()) {
    target_temp = *call.get_target_temperature();
  }
  if (call.get_mode().has_value()) {
    mode = *call.get_mode();
  }

  if (mode == climate::CLIMATE_MODE_HEAT) {
    if (isnan(target_temp) || target_temp == 0) {
      // If we want to turn on heat, temperature must be set to a valid value.
      target_temp = MAX_TEMPERATURE;
    }
    if (!this->transmit_temperature_(&target_temp)) {
      return;
    }
  } else {
    // Keep target_temp unchanged, only transmit a zero value which corresponds
    // to mode=off.
    float unused_target_temp = 0;
    if (!this->transmit_temperature_(&unused_target_temp)) {
      return;
    }
  }

  this->target_temperature = target_temp;
  this->mode = mode;
  this->publish_state();
}

void ZehnderComponent::setup() {
  this->target_temperature = 0;
  this->mode = climate::CLIMATE_MODE_OFF;
}

void ZehnderComponent::update() {
  float target_temp = this->mode == climate::CLIMATE_MODE_HEAT ?
    this->target_temperature :
    0;
  this->transmit_temperature_(&target_temp);
}

climate::ClimateTraits ZehnderComponent::traits() {
  auto traits = climate::ClimateTraits();

  traits.set_visual_min_temperature(MIN_TEMPERATURE);
  traits.set_visual_max_temperature(MAX_TEMPERATURE);
  // Must be
  // (MAX_TEMPERATURE - MIN_TEMPERATURE) / (TEMPERATURE_LEVELS - 1)
  // but no longer supported by HA, because this component
  // uses the same value to transmit precision.
  traits.set_visual_temperature_step(1);

  if (has_temperature_sensor_) {
    traits.set_supports_current_temperature(true);
  }

  traits.set_supported_modes({
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_OFF
  });

  return traits;
}

void ZehnderComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "Zehnder:");
  ESP_LOGCONFIG(TAG, "  has temperature sensor: %s", this->has_temperature_sensor_ ? "yes" : "no");
}

bool ZehnderComponent::transmit_temperature_(float *temp) {
  uint8_t level;
  if (*temp == 0) {
    level = 0;
  } else {
    level =
      (*temp - MIN_TEMPERATURE) *
      (TEMPERATURE_LEVELS - 1) /
      (MAX_TEMPERATURE - MIN_TEMPERATURE) +
      1;
    if (level > TEMPERATURE_LEVELS) {
      level = TEMPERATURE_LEVELS;
    }

    *temp =
      (MAX_TEMPERATURE - MIN_TEMPERATURE) /
      (TEMPERATURE_LEVELS - 1) *
      (level - 1) +
      MIN_TEMPERATURE;
  }
  return this->transmit_level_(level);
}

bool ZehnderComponent::transmit_level_(uint8_t level) {
  if (level > TEMPERATURE_LEVELS) {
    ESP_LOGE(TAG, "Unsupported heater level: %d. Ignoring command", level);
    return false;
  }
  if (this->transmitter_ == nullptr) {
    ESP_LOGE(TAG, "IR transmitter not set. Use set_transmitter.");
    return false;
  }

  ESP_LOGD(TAG, "Transmitting heater level: %d...", level);

  this->transmitter_->set_carrier_duty_percent(50);
  auto call = this->transmitter_->transmit();
  call.set_send_times(10);
  call.set_send_wait(10);  // milliseconds
  auto data = call.get_data();

  data->reset();
  data->set_carrier_frequency(455000);

  static uint8_t mode_bytes[TEMPERATURE_LEVELS + 1] = {
    0x0D,  // OFF
    0x86,
    0x95,
    0xA0,
    0xB3,
    0xCA,
    0xD9,
    0xEC,
    0xFF
  };

  add_header_(data);
  add_byte_(data, 0xB8);
  add_byte_(data, mode_bytes[level]);
  add_post_(data);

  call.perform();
  return true;
}

}  // namespace zehnder
}  // namespace esphome

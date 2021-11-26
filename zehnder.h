#include "esphome.h"

namespace {
  static const char* const TAG = "zehnder";
  static const uint8_t MIN_TEMPERATURE = 35;
  static const uint8_t MAX_TEMPERATURE = 70;
  static const uint8_t TEMPERATURE_LEVELS = 8;
}

class Zehnder : public Component, public Climate {
 public:
  void retransmit() {
    this->transmit_temperature_(this->target_temperature);
  }

  void control(const ClimateCall &call) override {
    if (call.get_mode().has_value()) {
      ClimateMode mode = *call.get_mode();
      if (mode == climate::CLIMATE_MODE_OFF) {
        if (this->transmit_level_(0)) {
          this->target_temperature = 0;
          this->mode = mode;
        }
      } else {
        if (this->transmit_temperature_(MAX_TEMPERATURE)) {
          this->target_temperature = MAX_TEMPERATURE;
          this->mode = mode;
        }
      }
    }
    if (call.get_target_temperature().has_value()) {
      float temp = *call.get_target_temperature();
      if (this->transmit_temperature_(temp)) {
        this->target_temperature = temp;
      }
    }
    this->publish_state();
  }

  ClimateTraits traits() override {
    auto traits = climate::ClimateTraits();

    traits.set_visual_min_temperature(MIN_TEMPERATURE);
    traits.set_visual_max_temperature(MAX_TEMPERATURE);
    traits.set_visual_temperature_step(
      (MAX_TEMPERATURE - MIN_TEMPERATURE) / (TEMPERATURE_LEVELS - 1)
    );

    traits.set_supported_modes({
      climate::CLIMATE_MODE_HEAT,
      climate::CLIMATE_MODE_OFF
    });

    return traits;
  }

  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }

 private:
  remote_transmitter::RemoteTransmitterComponent *transmitter_;

  bool transmit_temperature_(float temp) {
    return this->transmit_level_(temp == 0
      ? 0
      : (
        (temp - MIN_TEMPERATURE) *
        (TEMPERATURE_LEVELS - 1) /
        (MAX_TEMPERATURE - MIN_TEMPERATURE) +
        1
      )
    );
  }

  bool transmit_level_(uint8_t level) {
    if (level > 8) {
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

  static void add_header_(remote_base::RemoteTransmitData *data) {
    data->item(30, 1000);
  }

  static void add_byte_(remote_base::RemoteTransmitData *data, uint8_t value) {
    for (int i = 0; i < 8; ++i) {
      data->item(30, value & 0b10000000 ? 830 : 650);
      value <<= 1;
    }
  }

  static void add_post_(remote_base::RemoteTransmitData *data) {
    data->item(30, 460);
    data->item(30, 650);
  }
};

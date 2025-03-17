#pragma once

#include "esphome/components/climate/climate.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace zehnder {

class ZehnderComponent : public climate::Climate,
                         public PollingComponent {
 public:
  explicit ZehnderComponent(): Climate() {};

  // Component.
  void setup() override;
  float get_setup_priority() const override {
    return setup_priority::HARDWARE - 1.0f;
  }
  void dump_config() override;

  // PollingComponent.
  void update() override;

  void set_transmitter(remote_transmitter::RemoteTransmitterComponent *t) {
    this->transmitter_ = t;
  }

  void add_temperature_sensor(sensor::Sensor *s) {
    has_temperature_sensor_ = true;
    s->add_on_state_callback([this](float temperature_value) {
      this->current_temperature = temperature_value;
      this->publish_state();
    });
  }

 protected:
  // climate::Climate.
  void control(const climate::ClimateCall &call) override;
  climate::ClimateTraits traits() override;

  bool transmit_temperature_(float *temp);
  bool transmit_level_(uint8_t level);

  remote_transmitter::RemoteTransmitterComponent *transmitter_ = nullptr;
  bool has_temperature_sensor_ = false;
};

}  // namespace zehnder
}  // namespace esphome

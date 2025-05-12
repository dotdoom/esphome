#pragma once

#include <string>

#include "esphome/components/cover/cover.h"
#include "esphome/components/mqtt/mqtt_component.h"
#include "esphome/components/remote_transmitter/remote_transmitter.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace somfy {

class SomfyRTSCover : public cover::Cover, public Component {
 public:
  void set_transmitter(
      remote_transmitter::RemoteTransmitterComponent *transmitter) {
    this->transmitter_ = transmitter;
  }
  void set_remote_id(uint32_t remote_id) { this->remote_id_ = remote_id; }

  cover::CoverTraits get_traits() override;
  void setup() override;
  void dump_config() override;
  void control(const cover::CoverCall &call) override;

 protected:
  /* Wake up the blinds motor controller and send the command.
   * Overall, with repeats and syncs, takes about 510ms.
   *
   * mqtt.publish is synchronous unless idf_send_async is set, and may
   * introduce an unknown delay into the method execution timeline.
   */
  void send(uint8_t command);

  uint32_t remote_id_;
  uint32_t rolling_code_;
  std::string mqtt_topic_prefix_;
  ESPPreferenceObject rolling_code_pref_;
  remote_transmitter::RemoteTransmitterComponent *transmitter_{nullptr};
};

}  // namespace somfy
}  // namespace esphome

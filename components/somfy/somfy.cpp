#include "somfy.h"

#include "cc1101.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome {
namespace somfy {
static const char *const TAG = "somfy";

static const uint8_t CMD_STOP = 0x01;
static const uint8_t CMD_UP = 0x02;
static const uint8_t CMD_DOWN = 0x04;
static const uint8_t CMD_PROG = 0x08;

static const int RF_SYMBOL = 640;

void SomfyRTSCover::dump_config() {
  LOG_COVER("", "Somfy RTS Cover", this);
  ESP_LOGCONFIG(TAG, "  RF Pin: %d", this->rf_pin_);
  ESP_LOGCONFIG(TAG, "  Remote ID: %d", this->remote_id_);
  if (this->rolling_code_) {
    ESP_LOGCONFIG(TAG, "  Rolling Code: %d", this->rolling_code_);
  } else {
    ESP_LOGCONFIG(TAG, "  Rolling Code: N/A");
  }
}

cover::CoverTraits SomfyRTSCover::get_traits() {
  auto traits = cover::CoverTraits();
  traits.set_is_assumed_state(true);
  traits.set_supports_position(false);
  traits.set_supports_tilt(false);
  traits.set_supports_toggle(false);
  return traits;
}

void SomfyRTSCover::setup() {
  pinMode(rf_pin_, OUTPUT);
  digitalWrite(rf_pin_, LOW);
  cc1101_Init();

  rolling_code_pref_ =
      global_preferences->make_preference<int>(this->get_object_id_hash());

  if (mqtt::global_mqtt_client != nullptr) {
    this->mqtt_topic_prefix_ =
        mqtt::global_mqtt_client->get_topic_prefix() + "/rolling_code/";
  }

  if (this->rolling_code_pref_.load(&this->rolling_code_)) {
    ESP_LOGI(TAG, "Restored rolling code for remote #%d from flash: %d",
             remote_id_, rolling_code_);
  } else if (mqtt::global_mqtt_client != nullptr) {
    // Either a new version was flashed, flash was corrupt, or this firmware
    // was flashed to a different ESP chip. Either way, try to restore from
    // MQTT.
    mqtt::global_mqtt_client->subscribe(
        mqtt_topic_prefix_ + std::to_string(remote_id_),
        [this](const std::string &topic, const std::string &payload) {
          if (rolling_code_ == 0) {
            ESP_LOGI(TAG, "Received rolling code for remote #%d from MQTT: %s",
                     remote_id_, payload.c_str());
            this->rolling_code_ = atoi(payload.c_str());
            this->rolling_code_pref_.save(&this->rolling_code_);
          }
        },
        /*qos=*/1 /* (at least once) */);
  } else {
    ESP_LOGE(TAG, "No rolling code in flash, and MQTT client unavailable");
    rolling_code_ = 0;
  }
}

void SomfyRTSCover::control(const cover::CoverCall &call) {
  if (call.get_position().has_value()) {
    float position = *call.get_position();
    send(position > 0.5 ? CMD_UP : CMD_DOWN);
    this->position = position;
    this->publish_state();
  } else if (call.get_stop()) {
    send(CMD_STOP);
  }
}

void SomfyRTSCover::send(uint8_t command) {
  if (rolling_code_ == 0) {
    ESP_LOGE(TAG,
             "Remote #%d was requested to run command %d, but has not "
             "yet received its rolling code. Refusing to proceed, to "
             "avoid crippling MQTT rolling code storage",
             remote_id_, command);
    return;
  }

  ESP_LOGI(TAG, "Remote #%d sending command %d with rolling code %d",
           remote_id_, command, rolling_code_);

  uint8_t frame[] = {
      0xA7,                                // Encryption key.
      static_cast<uint8_t>(command << 4),  // 4 lsb will be checksum.
      static_cast<uint8_t>(rolling_code_ >> 8),
      static_cast<uint8_t>(rolling_code_),
      static_cast<uint8_t>(remote_id_ >> 16),
      static_cast<uint8_t>(remote_id_ >> 8),
      static_cast<uint8_t>(remote_id_),
  };

  ++rolling_code_;

  // Checksum calculation: a XOR of all the nibbles.
  uint8_t checksum = 0;
  for (uint8_t i = 0; i < 7; i++) {
    checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
  }
  checksum &= 0b1111;  // We keep the last 4 bits only.
  frame[1] |= checksum;

  // Obfuscation: a XOR of all the bytes.
  for (uint8_t i = 1; i < 7; i++) {
    frame[i] ^= frame[i - 1];
  }

  // Wake-up pulse & silence.
  digitalWrite(rf_pin_, HIGH);
  delayMicroseconds(9415);
  digitalWrite(rf_pin_, LOW);
  delayMicroseconds(89565);

  for (int repeat = 0; repeat < 3; ++repeat) {
    // Hardware sync.
    uint8_t sync = repeat == 0 ? 2 : 7;
    for (uint8_t i = 0; i < sync; i++) {
      digitalWrite(rf_pin_, HIGH);
      delayMicroseconds(4 * RF_SYMBOL);
      digitalWrite(rf_pin_, LOW);
      delayMicroseconds(4 * RF_SYMBOL);
    }

    // Software sync.
    digitalWrite(rf_pin_, HIGH);
    delayMicroseconds(4550);
    digitalWrite(rf_pin_, LOW);
    delayMicroseconds(RF_SYMBOL);

    // Data: bits are sent one by one.
    for (uint8_t octet = 0; octet < 7; ++octet) {
      // Starting with MSB.
      for (signed char bit = 7; bit >= 0; --bit) {
        uint8_t value = (frame[octet] >> bit) & 1;
        if (value == 1) {
          digitalWrite(rf_pin_, LOW);
          delayMicroseconds(RF_SYMBOL);
          digitalWrite(rf_pin_, HIGH);
          delayMicroseconds(RF_SYMBOL);
        } else {
          digitalWrite(rf_pin_, HIGH);
          delayMicroseconds(RF_SYMBOL);
          digitalWrite(rf_pin_, LOW);
          delayMicroseconds(RF_SYMBOL);
        }
      }
    }

    digitalWrite(rf_pin_, LOW);
    // Inter-frame silence.
    delayMicroseconds(30415);
  }

  this->rolling_code_pref_.save(&rolling_code_);

  if (mqtt::global_mqtt_client != nullptr) {
    // Publish the new rolling code at the end to ensure fast reaction time
    // when the component is called. This method may be synchronous and
    // introduces an unpredictable delay.
    mqtt::global_mqtt_client->publish(
        mqtt_topic_prefix_ + std::to_string(remote_id_),
        std::to_string(rolling_code_),
        /*qos=*/1, /*retain=*/true);
  }
}

}  // namespace somfy
}  // namespace esphome

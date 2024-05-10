#include "esphome.h"

namespace {
  static const char* const TAG = __FILE__;

  static const uint8_t CMD_STOP = 0x01;
  static const uint8_t CMD_UP = 0x02;
  static const uint8_t CMD_DOWN = 0x04;
  static const uint8_t CMD_PROG = 0x08;

  static const int RF_SYMBOL = 640;
}

class SomfyRTS : public Component, public Cover {
  public:
    SomfyRTS(int rfPin, int remoteId, esphome::mqtt::MQTTClientComponent* mqtt) {
      this->rfPin = rfPin;
      this->remoteId = remoteId;
      this->mqtt = mqtt;
      this->mqttTopicPrefix = mqtt->get_topic_prefix() + "/rolling_code/";
    }

    void setup() override {
      pinMode(rfPin, OUTPUT);
      digitalWrite(rfPin, LOW);
      mqtt->subscribe(mqttTopicPrefix + std::to_string(remoteId),
                      [=](const std::string &topic, const std::string &payload) {
        if (rollingCode == 0) {
          ESP_LOGI(TAG, "Received rolling code for remote #%d from MQTT: %s",
                   remoteId, payload.c_str());
          this->rollingCode = atoi(payload.c_str());
        }
      }, /*qos=*/1 /* (at least once) */);
    }

    CoverTraits get_traits() override {
      auto traits = CoverTraits();
      traits.set_is_assumed_state(true);
      traits.set_supports_position(false);
      traits.set_supports_tilt(false);
      traits.set_supports_toggle(false);
      return traits;
    }

    void control(const CoverCall &call) override {
      if (call.get_position().has_value()) {
        float position = *call.get_position();
        send(position > 0.5 ? CMD_UP : CMD_DOWN);
        this->position = position;
        this->publish_state();
      } else if (call.get_stop()) {
        send(CMD_STOP);
      }
    }

  private:
    int rfPin;
    int remoteId;
    int rollingCode = 0;
    esphome::mqtt::MQTTClientComponent* mqtt;
    std::string mqttTopicPrefix;

    /* Wake up the blinds motor controller and send the command.
     * Overall, with repeats and syncs, takes about 510ms.
     *
     * mqtt.publish is synchronous unless idf_send_async is set, and may
     * introduce an unknown delay into the method execution timeline.
     */
    void send(uint8_t command) {
      if (rollingCode == 0) {
        ESP_LOGE(TAG, "Remote #%d was requested to run command %d, but has not "
                      "yet received its rolling code. Refusing to proceed, to "
                      "avoid crippling MQTT rolling code storage",
                 remoteId, command);
        return;
      }

      ESP_LOGI(TAG, "Remote #%d sending command %d with rolling code %d",
               remoteId, command, rollingCode);

      uint8_t frame[] = {
        0xA7,  // Encryption key.
        static_cast<uint8_t>(command << 4),  // 4 lsb will be checksum.

        static_cast<uint8_t>(rollingCode >> 8),
        static_cast<uint8_t>(rollingCode),

        static_cast<uint8_t>(remoteId >> 16),
        static_cast<uint8_t>(remoteId >> 8),
        static_cast<uint8_t>(remoteId),
      };

      ++rollingCode;

      // Checksum calculation: a XOR of all the nibbles.
      uint8_t checksum = 0;
      for (uint8_t i = 0; i < 7; i++) {
        checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
      }
      checksum &= 0b1111; // We keep the last 4 bits only.
      frame[1] |= checksum;

      // Obfuscation: a XOR of all the bytes.
      for (uint8_t i = 1; i < 7; i++) {
        frame[i] ^= frame[i-1];
      }

      // Wake-up pulse & silence.
      digitalWrite(rfPin, HIGH);
      delayMicroseconds(9415);
      digitalWrite(rfPin, LOW);
      delayMicroseconds(89565);

      for (int repeat = 0; repeat < 3; ++repeat) {
        // Hardware sync.
        uint8_t sync = repeat == 0 ? 2 : 7;
        for (uint8_t i = 0; i < sync; i++) {
          digitalWrite(rfPin, HIGH);
          delayMicroseconds(4*RF_SYMBOL);
          digitalWrite(rfPin, LOW);
          delayMicroseconds(4*RF_SYMBOL);
        }

        // Software sync.
        digitalWrite(rfPin, HIGH);
        delayMicroseconds(4550);
        digitalWrite(rfPin, LOW);
        delayMicroseconds(RF_SYMBOL);

        // Data: bits are sent one by one.
        for (uint8_t octet = 0; octet < 7; ++octet) {
          // Starting with MSB.
          for (signed char bit = 7; bit >= 0; --bit) {
            uint8_t value = (frame[octet] >> bit) & 1;
            if (value == 1) {
              digitalWrite(rfPin, LOW);
              delayMicroseconds(RF_SYMBOL);
              digitalWrite(rfPin, HIGH);
              delayMicroseconds(RF_SYMBOL);
            } else {
              digitalWrite(rfPin, HIGH);
              delayMicroseconds(RF_SYMBOL);
              digitalWrite(rfPin, LOW);
              delayMicroseconds(RF_SYMBOL);
            }
          }
        }

        digitalWrite(rfPin, LOW);
        // Inter-frame silence.
        delayMicroseconds(30415);
      }

      // Publish the new rolling code at the end to ensure fast reaction time
      // when the component is called. This method may be synchronous and
      // introduces an unpredictable delay.
      mqtt->publish(mqttTopicPrefix + std::to_string(remoteId),
                    std::to_string(rollingCode), /*qos=*/1, /*retain=*/true);
    }
};

#include "esphome.h"

namespace {
  static const char* const TAG = __FILE__;

  static const byte CMD_STOP = 0x01;
  static const byte CMD_UP = 0x02;
  static const byte CMD_DOWN = 0x04;
  static const byte CMD_PROG = 0x08;
  static const std::string ROLLING_CODE_PREFIX = "rolling_code/";

  static const int RF_SYMBOL = 640;
}

class SomfyRTS : public Component, public Cover {
  public:
    SomfyRTS(int rfPin, int remoteId, esphome::mqtt::MQTTClientComponent* mqtt) {
      this->rfPin = rfPin;
      this->remoteId = remoteId;
      this->mqtt = mqtt;
    }

    void setup() override {
      pinMode(rfPin, OUTPUT);
      digitalWrite(rfPin, LOW);
      mqtt->subscribe(ROLLING_CODE_PREFIX + std::to_string(remoteId),
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
      traits.set_supports_position(true);
      traits.set_supports_tilt(true);
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

    /* Wake up the blinds motor controller and send the command.
     * Overall, with repeats and syncs, takes about 510ms.
     *
     * mqtt.publish is synchronous unless idf_send_async is set, and may
     * introduce an unknown delay into the method execution timeline.
     */
    void send(byte command) {
      if (rollingCode == 0) {
        ESP_LOGE(TAG, "Remote #%d was requested to run command %d, but has not "
                      "yet received its rolling code. Refusing to proceed, to "
                      "avoid crippling MQTT rolling code storage",
                 remoteId, command);
        return;
      }

      ESP_LOGI(TAG, "Remote #%d sending command %d with rolling code %d",
               remoteId, command, rollingCode);

      byte frame[] = {
        0xA7,          // Encryption key.
        command << 4,  // 4 lsb will be checksum.

        rollingCode >> 8,
        rollingCode,

        remoteId >> 16,
        remoteId >> 8,
        remoteId,
      };

      ++rollingCode;

      // Checksum calculation: a XOR of all the nibbles.
      byte checksum = 0;
      for (byte i = 0; i < 7; i++) {
        checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
      }
      checksum &= 0b1111; // We keep the last 4 bits only.
      frame[1] |= checksum;

      // Obfuscation: a XOR of all the bytes.
      for (byte i = 1; i < 7; i++) {
        frame[i] ^= frame[i-1];
      }

      // Wake-up pulse & silence.
      digitalWrite(rfPin, HIGH);
      delayMicroseconds(9415);
      digitalWrite(rfPin, LOW);
      delayMicroseconds(89565);

      for (int repeat = 0; repeat < 3; ++repeat) {
        // Hardware sync.
        byte sync = repeat == 0 ? 2 : 7;
        for (byte i = 0; i < sync; i++) {
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
        for (byte octet = 0; octet < 7; ++octet) {
          // Starting with MSB.
          for (signed char bit = 7; bit >= 0; --bit) {
            byte value = (frame[octet] >> bit) & 1;
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
      mqtt->publish(ROLLING_CODE_PREFIX + std::to_string(remoteId),
                    std::to_string(rollingCode), /*qos=*/1, /*retain=*/true);
    }
};

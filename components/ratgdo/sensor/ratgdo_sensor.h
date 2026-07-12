#pragma once

#include "../ratgdo.h"
#include "../ratgdo_state.h"
#include "Wire.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "vl53l4cx_class.h"
#define I2C Wire

namespace esphome::ratgdo {

enum RATGDOSensorType : uint8_t { RATGDO_OPENINGS, RATGDO_DISTANCE = 6 };

class RATGDOSensor : public sensor::Sensor,
                     public RATGDOClient,
                     public Component {
 public:
  void dump_config() override;
  void setup() override;
  void loop() override;
  void set_ratgdo_sensor_type(RATGDOSensorType ratgdo_sensor_type_) {
    this->ratgdo_sensor_type_ = ratgdo_sensor_type_;
  }

 protected:
  RATGDOSensorType ratgdo_sensor_type_;

  VL53L4CX distance_sensor_;
};

}  // namespace esphome::ratgdo

#pragma once

#include "../ratgdo.h"
#include "../ratgdo_state.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"

#ifdef RATGDO_USE_DISTANCE_SENSOR
#include "Wire.h"
#include "vl53l4cx_class.h"
#define I2C Wire
#endif

namespace esphome::ratgdo {

enum RATGDOSensorType : uint8_t {
    RATGDO_OPENINGS,
    RATGDO_DISTANCE = 6
};

class RATGDOSensor : public sensor::Sensor, public RATGDOClient, public Component {
public:
    void dump_config() override;
    void setup() override;
#ifdef RATGDO_USE_DISTANCE_SENSOR
    void loop() override;
#endif
    void set_ratgdo_sensor_type(RATGDOSensorType ratgdo_sensor_type_) { this->ratgdo_sensor_type_ = ratgdo_sensor_type_; }

protected:
    RATGDOSensorType ratgdo_sensor_type_;

#ifdef RATGDO_USE_DISTANCE_SENSOR
    VL53L4CX distance_sensor_;
#endif
};

} // namespace esphome::ratgdo

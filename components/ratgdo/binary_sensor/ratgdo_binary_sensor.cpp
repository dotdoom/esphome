#include "ratgdo_binary_sensor.h"
#include "../ratgdo_state.h"
#include "esphome/core/log.h"

namespace esphome::ratgdo {

static const char* const TAG = "ratgdo.binary_sensor";

void RATGDOBinarySensor::setup()
{
    // Initialize all sensors to false
    this->publish_initial_state(false);

    switch (this->binary_sensor_type_) {
#ifdef RATGDO_USE_VEHICLE_SENSORS
    case SensorType::RATGDO_SENSOR_VEHICLE_DETECTED:
        this->parent_->subscribe_vehicle_detected_state([this](VehicleDetectedState state) {
            this->publish_state(state == VehicleDetectedState::YES);
            this->parent_->presence_change(state == VehicleDetectedState::YES);
        });
        break;
    case SensorType::RATGDO_SENSOR_VEHICLE_ARRIVING:
        this->parent_->subscribe_vehicle_arriving_state([this](VehicleArrivingState state) {
            this->publish_state(state == VehicleArrivingState::YES);
        });
        break;
    case SensorType::RATGDO_SENSOR_VEHICLE_LEAVING:
        this->parent_->subscribe_vehicle_leaving_state([this](VehicleLeavingState state) {
            this->publish_state(state == VehicleLeavingState::YES);
        });
        break;
#endif
    default:
        break;
    }
}

void RATGDOBinarySensor::dump_config()
{
    LOG_BINARY_SENSOR("", "RATGDO BinarySensor", this);
    switch (this->binary_sensor_type_) {
#ifdef RATGDO_USE_VEHICLE_SENSORS
    case SensorType::RATGDO_SENSOR_VEHICLE_DETECTED:
        ESP_LOGCONFIG(TAG, "  Type: VehicleDetected");
        break;
    case SensorType::RATGDO_SENSOR_VEHICLE_ARRIVING:
        ESP_LOGCONFIG(TAG, "  Type: VehicleArriving");
        break;
    case SensorType::RATGDO_SENSOR_VEHICLE_LEAVING:
        ESP_LOGCONFIG(TAG, "  Type: VehicleLeaving");
        break;
#endif
    default:
        break;
    }
}

} // namespace esphome::ratgdo

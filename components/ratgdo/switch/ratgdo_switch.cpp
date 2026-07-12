#include "ratgdo_switch.h"

#include "../ratgdo_state.h"
#include "esphome/core/log.h"

namespace esphome::ratgdo {

static const char* const TAG = "ratgdo.switch";

void RATGDOSwitch::dump_config() {
  LOG_SWITCH("", "RATGDO Switch", this);
  switch (this->switch_type_) {
    case SwitchType::RATGDO_LED:
      ESP_LOGCONFIG(TAG, "  Type: LED");
      break;
    default:
      break;
  }
}

void RATGDOSwitch::setup() {
  switch (this->switch_type_) {
    case SwitchType::RATGDO_LED:
      this->pin_->setup();
      break;
    default:
      break;
  }
}

void RATGDOSwitch::write_state(bool state) {
  switch (this->switch_type_) {
    case SwitchType::RATGDO_LED:
      this->pin_->digital_write(state);
      this->publish_state(state);
      break;
    default:
      break;
  }
}

}  // namespace esphome::ratgdo

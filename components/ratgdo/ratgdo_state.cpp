#include "ratgdo_state.h"

namespace esphome::ratgdo {

LightState light_state_toggle(LightState state) {
  switch (state) {
    case LightState::OFF:
      return LightState::ON;
    case LightState::ON:
      return LightState::OFF;
      // 2 and 3 appears sometimes
    case LightState::UNKNOWN:
    default:
      return LightState::UNKNOWN;
  }
}

}  // namespace esphome::ratgdo

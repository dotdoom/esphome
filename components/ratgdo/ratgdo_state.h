
/************************************
 * Copyright (C) 2022  Paul Wieland
 *
 * GNU GENERAL PUBLIC LICENSE
 ************************************/

#pragma once
#include <cstdint>

#include "esphome/core/defines.h"

namespace esphome::ratgdo {

enum class DoorState : uint8_t {
  UNKNOWN = 0,
  OPEN = 1,
  CLOSED = 2,
  STOPPED = 3,
  OPENING = 4,
  CLOSING = 5
};

inline const char* DoorState_to_string(DoorState e) {
  static const char* const names[] = {"UNKNOWN", "OPEN",    "CLOSED",
                                      "STOPPED", "OPENING", "CLOSING"};
  auto i = static_cast<uint8_t>(e);
  return (i <= static_cast<uint8_t>(DoorState::CLOSING)) ? names[i] : "UNKNOWN";
}

inline DoorState to_DoorState(uint8_t t, DoorState unknown) {
  return (t <= static_cast<uint8_t>(DoorState::CLOSING))
             ? static_cast<DoorState>(t)
             : unknown;
}

/// Enum for all states a the light can be in.
enum class LightState : uint8_t { OFF = 0, ON = 1, UNKNOWN = 2 };

inline const char* LightState_to_string(LightState e) {
  static const char* const names[] = {"OFF", "ON", "UNKNOWN"};
  auto i = static_cast<uint8_t>(e);
  return (i <= static_cast<uint8_t>(LightState::UNKNOWN)) ? names[i]
                                                          : "UNKNOWN";
}

inline LightState to_LightState(uint8_t t, LightState unknown) {
  return (t <= static_cast<uint8_t>(LightState::UNKNOWN))
             ? static_cast<LightState>(t)
             : unknown;
}

LightState light_state_toggle(LightState state);

/// Enum for all states a the lock can be in.
enum class LockState : uint8_t { UNLOCKED = 0, LOCKED = 1, UNKNOWN = 2 };

inline const char* LockState_to_string(LockState e) {
  static const char* const names[] = {"UNLOCKED", "LOCKED", "UNKNOWN"};
  auto i = static_cast<uint8_t>(e);
  return (i <= static_cast<uint8_t>(LockState::UNKNOWN)) ? names[i] : "UNKNOWN";
}

inline LockState to_LockState(uint8_t t, LockState unknown) {
  return (t <= static_cast<uint8_t>(LockState::UNKNOWN))
             ? static_cast<LockState>(t)
             : unknown;
}

// actions
enum class LightAction : uint8_t { OFF = 0, ON = 1, TOGGLE = 2, UNKNOWN = 3 };

inline const char* LightAction_to_string(LightAction e) {
  static const char* const names[] = {"OFF", "ON", "TOGGLE", "UNKNOWN"};
  auto i = static_cast<uint8_t>(e);
  return (i <= static_cast<uint8_t>(LightAction::UNKNOWN)) ? names[i]
                                                           : "UNKNOWN";
}

inline LightAction to_LightAction(uint8_t t, LightAction unknown) {
  return (t <= static_cast<uint8_t>(LightAction::UNKNOWN))
             ? static_cast<LightAction>(t)
             : unknown;
}

enum class LockAction : uint8_t { UNLOCK = 0, LOCK = 1, UNKNOWN = 3 };

inline const char* LockAction_to_string(LockAction e) {
  static const char* const names[] = {"UNLOCK", "LOCK", "UNKNOWN", "UNKNOWN"};
  auto i = static_cast<uint8_t>(e);
  return (i <= static_cast<uint8_t>(LockAction::UNKNOWN)) ? names[i]
                                                          : "UNKNOWN";
}

inline LockAction to_LockAction(uint8_t t, LockAction unknown) {
  return (t == static_cast<uint8_t>(LockAction::UNLOCK) ||
          t == static_cast<uint8_t>(LockAction::LOCK) ||
          t == static_cast<uint8_t>(LockAction::UNKNOWN))
             ? static_cast<LockAction>(t)
             : unknown;
}

enum class DoorAction : uint8_t {
  CLOSE = 0,
  OPEN = 1,
  TOGGLE = 2,
  STOP = 3,
  UNKNOWN = 4
};

inline const char* DoorAction_to_string(DoorAction e) {
  static const char* const names[] = {"CLOSE", "OPEN", "TOGGLE", "STOP",
                                      "UNKNOWN"};
  auto i = static_cast<uint8_t>(e);
  return (i <= static_cast<uint8_t>(DoorAction::UNKNOWN)) ? names[i]
                                                          : "UNKNOWN";
}

inline DoorAction to_DoorAction(uint8_t t, DoorAction unknown) {
  return (t <= static_cast<uint8_t>(DoorAction::UNKNOWN))
             ? static_cast<DoorAction>(t)
             : unknown;
}

struct Openings {
  uint16_t count;
  uint8_t flag;
};

}  // namespace esphome::ratgdo

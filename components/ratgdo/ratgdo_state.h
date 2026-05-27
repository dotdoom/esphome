/************************************
 * Rage
 * Against
 * The
 * Garage
 * Door
 * Opener
 *
 * Copyright (C) 2022  Paul Wieland
 *
 * GNU GENERAL PUBLIC LICENSE
 ************************************/

#pragma once
#include "esphome/core/defines.h"
#include <cstdint>

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
    switch (e) {
        case DoorState::UNKNOWN: return "UNKNOWN";
        case DoorState::OPEN: return "OPEN";
        case DoorState::CLOSED: return "CLOSED";
        case DoorState::STOPPED: return "STOPPED";
        case DoorState::OPENING: return "OPENING";
        case DoorState::CLOSING: return "CLOSING";
        default: return "UNKNOWN";
    }
}

inline DoorState to_DoorState(uint8_t t, DoorState unknown) {
    switch (t) {
        case static_cast<uint8_t>(DoorState::UNKNOWN): return DoorState::UNKNOWN;
        case static_cast<uint8_t>(DoorState::OPEN): return DoorState::OPEN;
        case static_cast<uint8_t>(DoorState::CLOSED): return DoorState::CLOSED;
        case static_cast<uint8_t>(DoorState::STOPPED): return DoorState::STOPPED;
        case static_cast<uint8_t>(DoorState::OPENING): return DoorState::OPENING;
        case static_cast<uint8_t>(DoorState::CLOSING): return DoorState::CLOSING;
        default: return unknown;
    }
}

/// Enum for all states a the light can be in.
enum class LightState : uint8_t {
    OFF = 0,
    ON = 1,
    UNKNOWN = 2
};

inline const char* LightState_to_string(LightState e) {
    switch (e) {
        case LightState::OFF: return "OFF";
        case LightState::ON: return "ON";
        case LightState::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

inline LightState to_LightState(uint8_t t, LightState unknown) {
    switch (t) {
        case static_cast<uint8_t>(LightState::OFF): return LightState::OFF;
        case static_cast<uint8_t>(LightState::ON): return LightState::ON;
        case static_cast<uint8_t>(LightState::UNKNOWN): return LightState::UNKNOWN;
        default: return unknown;
    }
}

LightState light_state_toggle(LightState state);

/// Enum for all states a the lock can be in.
enum class LockState : uint8_t {
    UNLOCKED = 0,
    LOCKED = 1,
    UNKNOWN = 2
};

inline const char* LockState_to_string(LockState e) {
    switch (e) {
        case LockState::UNLOCKED: return "UNLOCKED";
        case LockState::LOCKED: return "LOCKED";
        case LockState::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

inline LockState to_LockState(uint8_t t, LockState unknown) {
    switch (t) {
        case static_cast<uint8_t>(LockState::UNLOCKED): return LockState::UNLOCKED;
        case static_cast<uint8_t>(LockState::LOCKED): return LockState::LOCKED;
        case static_cast<uint8_t>(LockState::UNKNOWN): return LockState::UNKNOWN;
        default: return unknown;
    }
}

// actions
enum class LightAction : uint8_t {
    OFF = 0,
    ON = 1,
    TOGGLE = 2,
    UNKNOWN = 3
};

inline const char* LightAction_to_string(LightAction e) {
    switch (e) {
        case LightAction::OFF: return "OFF";
        case LightAction::ON: return "ON";
        case LightAction::TOGGLE: return "TOGGLE";
        case LightAction::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

inline LightAction to_LightAction(uint8_t t, LightAction unknown) {
    switch (t) {
        case static_cast<uint8_t>(LightAction::OFF): return LightAction::OFF;
        case static_cast<uint8_t>(LightAction::ON): return LightAction::ON;
        case static_cast<uint8_t>(LightAction::TOGGLE): return LightAction::TOGGLE;
        case static_cast<uint8_t>(LightAction::UNKNOWN): return LightAction::UNKNOWN;
        default: return unknown;
    }
}

enum class LockAction : uint8_t {
    UNLOCK = 0,
    LOCK = 1,
    UNKNOWN = 3
};

inline const char* LockAction_to_string(LockAction e) {
    switch (e) {
        case LockAction::UNLOCK: return "UNLOCK";
        case LockAction::LOCK: return "LOCK";
        case LockAction::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

inline LockAction to_LockAction(uint8_t t, LockAction unknown) {
    switch (t) {
        case static_cast<uint8_t>(LockAction::UNLOCK): return LockAction::UNLOCK;
        case static_cast<uint8_t>(LockAction::LOCK): return LockAction::LOCK;
        case static_cast<uint8_t>(LockAction::UNKNOWN): return LockAction::UNKNOWN;
        default: return unknown;
    }
}

enum class DoorAction : uint8_t {
    CLOSE = 0,
    OPEN = 1,
    TOGGLE = 2,
    STOP = 3,
    UNKNOWN = 4
};

inline const char* DoorAction_to_string(DoorAction e) {
    switch (e) {
        case DoorAction::CLOSE: return "CLOSE";
        case DoorAction::OPEN: return "OPEN";
        case DoorAction::TOGGLE: return "TOGGLE";
        case DoorAction::STOP: return "STOP";
        case DoorAction::UNKNOWN: return "UNKNOWN";
        default: return "UNKNOWN";
    }
}

inline DoorAction to_DoorAction(uint8_t t, DoorAction unknown) {
    switch (t) {
        case static_cast<uint8_t>(DoorAction::CLOSE): return DoorAction::CLOSE;
        case static_cast<uint8_t>(DoorAction::OPEN): return DoorAction::OPEN;
        case static_cast<uint8_t>(DoorAction::TOGGLE): return DoorAction::TOGGLE;
        case static_cast<uint8_t>(DoorAction::STOP): return DoorAction::STOP;
        case static_cast<uint8_t>(DoorAction::UNKNOWN): return DoorAction::UNKNOWN;
        default: return unknown;
    }
}

struct Openings {
    uint16_t count;
    uint8_t flag;
};

} // namespace esphome::ratgdo

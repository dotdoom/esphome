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
#include "macros.h"
#include <cstdint>

namespace esphome::ratgdo {

ENUM(DoorState, uint8_t,
    (UNKNOWN, 0),
    (OPEN, 1),
    (CLOSED, 2),
    (STOPPED, 3),
    (OPENING, 4),
    (CLOSING, 5))

ENUM(DoorActionDelayed, uint8_t,
    (NO, 0),
    (YES, 1))

/// Enum for all states a the light can be in.
ENUM(LightState, uint8_t,
    (OFF, 0),
    (ON, 1),
    (UNKNOWN, 2))
LightState light_state_toggle(LightState state);

/// Enum for all states a the lock can be in.
ENUM(LockState, uint8_t,
    (UNLOCKED, 0),
    (LOCKED, 1),
    (UNKNOWN, 2))

// actions
ENUM(LightAction, uint8_t,
    (OFF, 0),
    (ON, 1),
    (TOGGLE, 2),
    (UNKNOWN, 3))

ENUM(LockAction, uint8_t,
    (UNLOCK, 0),
    (LOCK, 1),
    (UNKNOWN, 3))

ENUM(DoorAction, uint8_t,
    (CLOSE, 0),
    (OPEN, 1),
    (TOGGLE, 2),
    (STOP, 3),
    (UNKNOWN, 4))

struct Openings {
    uint16_t count;
    uint8_t flag;
};

} // namespace esphome::ratgdo

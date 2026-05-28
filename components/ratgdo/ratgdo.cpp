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

#include "ratgdo.h"
#include "common.h"
#include "ratgdo_state.h"

#include "secplus2.h"

#include "esphome/core/application.h"
#include "esphome/core/gpio.h"
#include "esphome/core/log.h"

namespace esphome::ratgdo {



static const char* const TAG = "ratgdo";
static constexpr int SYNC_DELAY = 5000;
// Door state updates arrive over UART every ~200-400ms during movement.
// 2 seconds gives ample margin for slow openers while still expiring
// stale callbacks before a user could reasonably trigger an unrelated
// door state change.
static constexpr uint32_t DOOR_STATE_CALLBACK_TIMEOUT = 2000;

using namespace scheduler_ids;

void log_subscriber_overflow(const LogString* observable_name, uint32_t max)
{
    ESP_LOGE(TAG, "Too many subscribers for %s (max %d)",
        LOG_STR_ARG(observable_name), (int)max);
}

void RATGDOComponent::setup()
{
    this->output_gdo_pin_->setup();
    this->output_gdo_pin_->pin_mode(gpio::FLAG_OUTPUT);

    this->input_gdo_pin_->setup();
    this->input_gdo_pin_->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP);

    this->protocol_->setup(this, &App.scheduler, this->input_gdo_pin_,
        this->output_gdo_pin_);

    // many things happening at startup, use some delay for sync
    this->set_timeout(SYNC_DELAY, [this] { this->sync(); });
    ESP_LOGD(TAG, " _____ _____ _____ _____ ____  _____ ");
    ESP_LOGD(TAG, "| __  |  _  |_   _|   __|    \\|     |");
    ESP_LOGD(TAG, "|    -|     | | | |  |  |  |  |  |  |");
    ESP_LOGD(TAG, "|__|__|__|__| |_| |_____|____/|_____|");
    ESP_LOGD(TAG, "https://paulwieland.github.io/ratgdo/");
}

// initializing protocol, this gets called before setup() because
// its children components might require that
void RATGDOComponent::init_protocol()
{
    this->protocol_ = new secplus2::Secplus2();
}

void RATGDOComponent::loop()
{
    this->protocol_->loop();
}

void RATGDOComponent::dump_config()
{
    ESP_LOGCONFIG(TAG, "Setting up RATGDO...");
    LOG_PIN("  Output GDO Pin: ", this->output_gdo_pin_);
    LOG_PIN("  Input GDO Pin: ", this->input_gdo_pin_);
    this->protocol_->dump_config();
}

void RATGDOComponent::on_shutdown()
{
    if (this->protocol_ != nullptr) {
        this->protocol_->on_shutdown();
    }
}

void RATGDOComponent::received(const DoorState door_state)
{
    ESP_LOGD(TAG, "Door state=%s", LOG_STR_ARG(DoorState_to_string(door_state)));

    auto prev_door_state = *this->door_state;

    if (prev_door_state == door_state) {
        return;
    }

    // opening duration calibration
    if (*this->opening_duration == 0) {
        if (door_state == DoorState::OPENING && prev_door_state == DoorState::CLOSED) {
            this->start_opening = millis();
        }
        if (door_state == DoorState::OPEN && prev_door_state == DoorState::OPENING && this->start_opening > 0) {
            auto duration = (millis() - this->start_opening) / 1000;
            this->set_opening_duration(round(duration * 10) / 10);
        }
        if (door_state == DoorState::STOPPED) {
            this->start_opening = -1;
        }
    }
    // closing duration calibration
    if (*this->closing_duration == 0) {
        if (door_state == DoorState::CLOSING && prev_door_state == DoorState::OPEN) {
            this->start_closing = millis();
        }
        if (door_state == DoorState::CLOSED && prev_door_state == DoorState::CLOSING && this->start_closing > 0) {
            auto duration = (millis() - this->start_closing) / 1000;
            this->set_closing_duration(round(duration * 10) / 10);
        }
        if (door_state == DoorState::STOPPED) {
            this->start_closing = -1;
        }
    }

    if (door_state == DoorState::OPENING) {
        // door started opening
        if (prev_door_state == DoorState::CLOSING) {
            this->door_position_update();
            this->cancel_position_sync_callbacks();
            this->door_move_delta = DOOR_DELTA_UNKNOWN;
        }
        this->door_start_moving = millis();
        this->door_start_position = *this->door_position;
        if (this->door_move_delta == DOOR_DELTA_UNKNOWN) {
            this->door_move_delta = 1.0 - this->door_start_position;
        }
        if (*this->opening_duration != 0) {
            this->schedule_door_position_sync();
        }
    } else if (door_state == DoorState::CLOSING) {
        // door started closing
        if (prev_door_state == DoorState::OPENING) {
            this->door_position_update();
            this->cancel_position_sync_callbacks();
            this->door_move_delta = DOOR_DELTA_UNKNOWN;
        }
        this->door_start_moving = millis();
        this->door_start_position = *this->door_position;
        if (this->door_move_delta == DOOR_DELTA_UNKNOWN) {
            this->door_move_delta = 0.0 - this->door_start_position;
        }
        if (*this->closing_duration != 0) {
            this->schedule_door_position_sync();
        }
    } else if (door_state == DoorState::STOPPED) {
        this->door_position_update();
        if (*this->door_position == DOOR_POSITION_UNKNOWN) {
            this->door_position = 0.5; // best guess
        }
        this->cancel_position_sync_callbacks();
        this->cancel_timeout(TIMEOUT_DOOR_QUERY_STATE);
        this->target_position_ = DOOR_POSITION_UNKNOWN;
        this->target_direction_ = DoorAction::UNKNOWN;
    } else if (door_state == DoorState::OPEN) {
        this->door_position = 1.0;
        this->cancel_position_sync_callbacks();
        this->target_position_ = DOOR_POSITION_UNKNOWN;
        this->target_direction_ = DoorAction::UNKNOWN;
    } else if (door_state == DoorState::CLOSED) {
        this->door_position = 0.0;
        this->cancel_position_sync_callbacks();
        this->target_position_ = DOOR_POSITION_UNKNOWN;
        this->target_direction_ = DoorAction::UNKNOWN;
    }

    if (door_state == DoorState::CLOSED && door_state != prev_door_state) {
        this->query_openings();
    }

    this->door_state = door_state;
    this->on_door_state_.trigger(door_state);
}

void RATGDOComponent::received(const LightState light_state)
{
    ESP_LOGD(TAG, "Light state=%s",
        LOG_STR_ARG(LightState_to_string(light_state)));
    this->light_state = light_state;
}

void RATGDOComponent::received(const LockState lock_state)
{
    ESP_LOGD(TAG, "Lock state=%s", LOG_STR_ARG(LockState_to_string(lock_state)));
    this->lock_state = lock_state;
}

void RATGDOComponent::received(const LightAction light_action)
{
    ESP_LOGD(TAG, "Light cmd=%s state=%s",
        LOG_STR_ARG(LightAction_to_string(light_action)),
        LOG_STR_ARG(LightState_to_string(*this->light_state)));
    if (light_action == LightAction::OFF) {
        this->light_state = LightState::OFF;
    } else if (light_action == LightAction::ON) {
        this->light_state = LightState::ON;
    } else if (light_action == LightAction::TOGGLE) {
        this->light_state = light_state_toggle(*this->light_state);
    }
}

void RATGDOComponent::received(const Openings openings)
{
    if (openings.flag == 0 || *this->openings != 0) {
        this->openings = openings.count;
        ESP_LOGD(TAG, "Openings: %d", *this->openings);
    } else {
        ESP_LOGD(TAG, "Ignoring openings, not from our request");
    }
}

void RATGDOComponent::schedule_door_position_sync(float update_period)
{
    ESP_LOG1(
        TAG,
        "Schedule position sync: delta %f, start position: %f, start moving: %d",
        this->door_move_delta, this->door_start_position,
        this->door_start_moving);
    auto duration = this->door_move_delta > 0 ? *this->opening_duration
                                              : *this->closing_duration;
    if (duration == 0) {
        return;
    }
    this->position_sync_remaining_ = std::max(static_cast<uint16_t>(1000 * duration / update_period),
        static_cast<uint16_t>(1));
    set_interval(INTERVAL_POSITION_SYNC, static_cast<uint32_t>(update_period),
        [this]() {
            this->door_position_update();
            if (--this->position_sync_remaining_ == 0) {
                cancel_interval(INTERVAL_POSITION_SYNC);
            }
        });
}

void RATGDOComponent::door_position_update()
{
    if (this->door_start_moving == 0 || this->door_start_position == DOOR_POSITION_UNKNOWN || this->door_move_delta == DOOR_DELTA_UNKNOWN) {
        return;
    }
    auto now = millis();
    auto duration = this->door_move_delta > 0 ? *this->opening_duration
                                              : -*this->closing_duration;
    if (duration == 0) {
        return;
    }
    auto position = this->door_start_position + (now - this->door_start_moving) / (1000 * duration);
    position = clamp(position, 0.0f, 1.0f);
    ESP_LOG2(TAG, "[%d] Position update: %f", now, position);
    this->door_position = position;

    // Check if we reached our move-to-position target
    if (this->target_position_ != DOOR_POSITION_UNKNOWN && this->target_direction_ != DoorAction::UNKNOWN) {
        bool reached = false;
        if (this->target_direction_ == DoorAction::OPEN && position >= this->target_position_) {
            reached = true;
        } else if (this->target_direction_ == DoorAction::CLOSE && position <= this->target_position_) {
            reached = true;
        }

        if (reached) {
            ESP_LOGD(TAG, "Reached target position %.2f, stopping door", this->target_position_);
            this->door_stop();
            this->target_position_ = DOOR_POSITION_UNKNOWN;
            this->target_direction_ = DoorAction::UNKNOWN;
        }
    }
}

void RATGDOComponent::set_opening_duration(float duration)
{
    ESP_LOGD(TAG, "Set opening duration: %.1fs", duration);
    this->opening_duration = duration;
}

void RATGDOComponent::set_closing_duration(float duration)
{
    ESP_LOGD(TAG, "Set closing duration: %.1fs", duration);
    this->closing_duration = duration;
}

void RATGDOComponent::set_distance_measurement(int16_t distance)
{
    this->last_distance_measurement = distance;
}

void RATGDOComponent::query_status() { this->protocol_->query_status(); }

void RATGDOComponent::query_openings()
{
    this->protocol_->query_openings();
}

void RATGDOComponent::sync()
{
    this->protocol_->sync();
}

void RATGDOComponent::set_door_state_expiry()
{
    this->set_timeout(TIMEOUT_DOOR_STATE_EXPIRY, DOOR_STATE_CALLBACK_TIMEOUT,
        [this]() {
            ESP_LOGW(TAG, "Door state callback expired, clearing");
            this->on_door_state_.clear();
        });
}

void RATGDOComponent::cancel_door_state_expiry()
{
    this->cancel_timeout(TIMEOUT_DOOR_STATE_EXPIRY);
}

void RATGDOComponent::smart_door_action(DoorAction target_direction)
{
    // target_direction must be OPEN or CLOSE
    if (target_direction != DoorAction::OPEN && target_direction != DoorAction::CLOSE) {
        return;
    }

    DoorState expected_state = (target_direction == DoorAction::OPEN) ? DoorState::OPENING : DoorState::CLOSING;
    DoorState opposite_state = (target_direction == DoorAction::OPEN) ? DoorState::CLOSING : DoorState::OPENING;

    // 1. Try the discrete command first
    this->door_action(target_direction);

    // 2. Set a timeout to check if it worked
    this->set_timeout(2000, [this, target_direction, expected_state, opposite_state]() {
        if (*this->door_state == expected_state || 
            (*this->door_state == DoorState::OPEN && target_direction == DoorAction::OPEN) ||
            (*this->door_state == DoorState::CLOSED && target_direction == DoorAction::CLOSE)) {
            // It worked (or is already at destination)
            return;
        }

        ESP_LOGW(TAG, "Discrete %s command ignored. Falling back to TOGGLE...", 
            target_direction == DoorAction::OPEN ? "OPEN" : "CLOSE");

        // 3. Fallback: Send a TOGGLE
        this->door_action(DoorAction::TOGGLE);

        // 4. Check what the toggle did
        this->on_door_state([this, target_direction, expected_state, opposite_state](DoorState s) {
            if (s == opposite_state) {
                // Wrong direction! Stop it.
                ESP_LOGW(TAG, "Toggle went the wrong way. Stopping...");
                this->door_action(DoorAction::STOP);
                
                // Once stopped, toggle again to go the right way
                this->on_door_state([this](DoorState s2) {
                    if (s2 == DoorState::STOPPED) {
                        ESP_LOGD(TAG, "Stopped. Toggling again for correct direction.");
                        this->door_action(DoorAction::TOGGLE);
                    }
                });
            }
        });
    });
}

void RATGDOComponent::door_open()
{
    if (*this->door_state == DoorState::OPENING) {
        return; // gets ignored by opener
    }

    this->smart_door_action(DoorAction::OPEN);

    if (*this->opening_duration > 0) {
        // query state in case we don't get a status message
        this->set_timeout(
            TIMEOUT_DOOR_QUERY_STATE, (*this->opening_duration + 5) * 1000,
            [this]() {
                if (*this->door_state != DoorState::OPEN && *this->door_state != DoorState::STOPPED) {
                    this->received(DoorState::OPEN); // probably missed a status mesage,
                                                     // assume it's open
                    this->query_status(); // query in case we're wrong and it's stopped
                }
            });
    }
}

void RATGDOComponent::door_close()
{
    if (*this->door_state == DoorState::CLOSING) {
        return; // gets ignored by opener
    }

    if (*this->door_state == DoorState::OPENING) {
        // have to stop door first, otherwise close command is ignored
        this->door_action(DoorAction::STOP);
        this->on_door_state([this](DoorState s) {
            if (s == DoorState::STOPPED) {
                this->smart_door_action(DoorAction::CLOSE);
            } else {
                ESP_LOGW(TAG, "Door did not stop, ignoring close command");
            }
        });
        return;
    }

    this->smart_door_action(DoorAction::CLOSE);

    if (*this->closing_duration > 0) {
        // query state in case we don't get a status message
        this->set_timeout(
            TIMEOUT_DOOR_QUERY_STATE, (*this->closing_duration + 5) * 1000,
            [this]() {
                if (*this->door_state != DoorState::CLOSED && *this->door_state != DoorState::STOPPED) {
                    this->received(DoorState::CLOSED); // probably missed a status
                                                       // mesage, assume it's closed
                    this->query_status(); // query in case we're wrong and it's stopped
                }
            });
    }
}

void RATGDOComponent::door_stop()
{
    if (*this->door_state != DoorState::OPENING && *this->door_state != DoorState::CLOSING) {
        return;
    }
    this->door_action(DoorAction::STOP);
}

void RATGDOComponent::door_toggle() { this->door_action(DoorAction::TOGGLE); }

void RATGDOComponent::door_action(DoorAction action)
{
    this->protocol_->door_action(action);
}

void RATGDOComponent::door_move_to_position(float position)
{
    if (*this->door_state == DoorState::OPENING || *this->door_state == DoorState::CLOSING) {
        this->door_action(DoorAction::STOP);
        this->on_door_state([this, position](DoorState s) {
            if (s == DoorState::STOPPED) {
                this->door_move_to_position(position);
            }
        });
        return;
    }

    auto delta = position - *this->door_position;
    if (delta == 0) {
        ESP_LOGD(TAG, "Door is already at position %.2f", position);
        return;
    }

    auto duration = delta > 0 ? *this->opening_duration : -*this->closing_duration;
    if (duration == 0) {
        ESP_LOGW(TAG, "I don't know duration, ignoring move to position");
        return;
    }

    auto operation_time = 1000 * duration * delta;
    this->door_move_delta = delta;

    ESP_LOGD(TAG, "Moving to position %.2f (target duration %.1fs)", position,
        operation_time / 1000.0);

    this->target_position_ = position;
    this->target_direction_ = (delta > 0 ? DoorAction::OPEN : DoorAction::CLOSE);

    this->smart_door_action(this->target_direction_);
}

void RATGDOComponent::cancel_position_sync_callbacks()
{
    if (this->door_start_moving != 0) {
        ESP_LOGD(TAG, "Cancelling position callbacks");
        this->cancel_timeout(TIMEOUT_MOVE_TO_POSITION);
        cancel_interval(INTERVAL_POSITION_SYNC);

        this->door_start_moving = 0;
        this->door_start_position = DOOR_POSITION_UNKNOWN;
        this->door_move_delta = DOOR_DELTA_UNKNOWN;
    }
}

void RATGDOComponent::light_on()
{
    this->light_state = LightState::ON;
    this->protocol_->light_action(LightAction::ON);
}

void RATGDOComponent::light_off()
{
    this->light_state = LightState::OFF;
    this->protocol_->light_action(LightAction::OFF);
}

LightState RATGDOComponent::get_light_state() const
{
    return *this->light_state;
}

// Lock functions
void RATGDOComponent::lock()
{
    this->lock_state = LockState::LOCKED;
    this->protocol_->lock_action(LockAction::LOCK);
}

void RATGDOComponent::unlock()
{
    this->lock_state = LockState::UNLOCKED;
    this->protocol_->lock_action(LockAction::UNLOCK);
}

// Subscribe implementations are now templates in ratgdo.h

} // namespace esphome::ratgdo

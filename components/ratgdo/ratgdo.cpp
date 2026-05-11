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

#ifdef PROTOCOL_DRYCONTACT
#include "dry_contact.h"
#endif
#ifdef PROTOCOL_SECPLUSV1
#include "secplus1.h"
#endif
#ifdef PROTOCOL_SECPLUSV2
#include "secplus2.h"
#endif

#include "esphome/core/application.h"
#include "esphome/core/gpio.h"
#include "esphome/core/log.h"

namespace esphome::ratgdo {

using namespace protocol;

static const char* const TAG = "ratgdo";
static constexpr int SYNC_DELAY = 1000;
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

#ifdef RATGDO_USE_VEHICLE_SENSORS
static constexpr int CLEAR_PRESENCE = 60000; // how long to keep arriving/leaving active
static constexpr int PRESENCE_DETECT_WINDOW = 300000; // how long to calculate presence after door state change
static constexpr int PRESENCE_DETECT_WINDOW_AFTER_CLOSE = 15000; // how long to keep presence window active after door reaches closed

// increasing these values increases reliability but also increases detection
// time
static constexpr int PRESENCE_DETECTION_ON_THRESHOLD = 5; // Minimum percentage of valid bitset::in_range samples required to
                                                          // detect vehicle
static constexpr int PRESENCE_DETECTION_OFF_DEBOUNCE = 2; // The number of consecutive bitset::in_range iterations that must be 0
                                                          // before clearing vehicle detected state
#endif

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

    this->subscribe_door_state([this](DoorState state, float position) {
#ifdef RATGDO_USE_VEHICLE_SENSORS
        if (this->last_door_state_for_presence_ != DoorState::UNKNOWN && state != DoorState::CLOSED && !this->flags_.presence_detect_window_active) {
            this->flags_.presence_detect_window_active = true;
            this->set_timeout(
                TIMEOUT_PRESENCE_DETECT_WINDOW, PRESENCE_DETECT_WINDOW,
                [this] { this->flags_.presence_detect_window_active = false; });
        }

        if (state == DoorState::CLOSED) {
            this->set_timeout(
                TIMEOUT_PRESENCE_DETECT_WINDOW, PRESENCE_DETECT_WINDOW_AFTER_CLOSE,
                [this] { this->flags_.presence_detect_window_active = false; });
        }

        this->last_door_state_for_presence_ = state;
#endif
    });
}

// initializing protocol, this gets called before setup() because
// its children components might require that
void RATGDOComponent::init_protocol()
{
#ifdef PROTOCOL_SECPLUSV2
    this->protocol_ = new secplus2::Secplus2();
#endif
#ifdef PROTOCOL_SECPLUSV1
    this->protocol_ = new secplus1::Secplus1();
#endif
#ifdef PROTOCOL_DRYCONTACT
    this->protocol_ = new dry_contact::DryContact();
#endif
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
    } else if (door_state == DoorState::OPEN) {
        this->door_position = 1.0;
        this->cancel_position_sync_callbacks();
    } else if (door_state == DoorState::CLOSED) {
        this->door_position = 0.0;
        this->cancel_position_sync_callbacks();
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

void RATGDOComponent::received(const ButtonState button_state)
{
    ESP_LOGD(TAG, "Button state=%s",
        LOG_STR_ARG(ButtonState_to_string(*this->button_state)));
    this->button_state = button_state;
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

void RATGDOComponent::received(const TimeToClose ttc)
{
    ESP_LOGD(TAG, "Time to close (TTC): %ds", ttc.seconds);
}

void RATGDOComponent::received(const BatteryState battery_state)
{
    ESP_LOGD(TAG, "Battery state=%s",
        LOG_STR_ARG(BatteryState_to_string(battery_state)));
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
    ESP_LOG2(TAG, "[%d] Position update: %f", now, position);
    this->door_position = clamp(position, 0.0f, 1.0f);
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

#ifdef RATGDO_USE_DISTANCE_SENSOR
void RATGDOComponent::set_target_distance_measurement(int16_t distance)
{
    this->target_distance_measurement = distance;
}

void RATGDOComponent::set_distance_measurement(int16_t distance)
{
    this->last_distance_measurement = distance;

#ifdef RATGDO_USE_VEHICLE_SENSORS
    this->in_range <<= 1;
    this->in_range.set(0, distance <= *this->target_distance_measurement);
    this->calculate_presence();
#endif
}
#endif

#ifdef RATGDO_USE_VEHICLE_SENSORS
void RATGDOComponent::calculate_presence()
{
    int percent = this->in_range.count() * 100 / this->in_range.size();

    if (percent >= PRESENCE_DETECTION_ON_THRESHOLD)
        this->vehicle_detected_state = VehicleDetectedState::YES;

    if (percent == 0 && *this->vehicle_detected_state == VehicleDetectedState::YES) {
        this->presence_off_counter_++;
        ESP_LOGD(TAG, "Off counter: %d", this->presence_off_counter_);

        if (this->presence_off_counter_ / this->in_range.size() >= PRESENCE_DETECTION_OFF_DEBOUNCE) {
            this->presence_off_counter_ = 0;
            this->vehicle_detected_state = VehicleDetectedState::NO;
        }
    }

    if (percent != this->last_presence_percent_) {
        ESP_LOGD(TAG, "pct_in_range: %d", percent);
        this->last_presence_percent_ = percent;
        this->presence_off_counter_ = 0;
    }
    // ESP_LOGD(TAG, "in_range: %s", this->in_range.to_string().c_str());
}
#endif

#ifdef RATGDO_USE_VEHICLE_SENSORS
void RATGDOComponent::presence_change(bool sensor_value)
{
    if (this->flags_.presence_detect_window_active) {
        // Arriving and leaving are mutually exclusive — each branch clears the
        // other state. Sharing TIMEOUT_CLEAR_PRESENCE ensures that switching from
        // arriving to leaving (or vice versa) cancels the previous clear timeout,
        // which is correct since the previous state was already cleared above.
        if (sensor_value) {
            this->vehicle_arriving_state = VehicleArrivingState::YES;
            this->vehicle_leaving_state = VehicleLeavingState::NO;
            this->set_timeout(TIMEOUT_CLEAR_PRESENCE, CLEAR_PRESENCE, [this] {
                this->vehicle_arriving_state = VehicleArrivingState::NO;
            });
        } else {
            this->vehicle_arriving_state = VehicleArrivingState::NO;
            this->vehicle_leaving_state = VehicleLeavingState::YES;
            this->set_timeout(TIMEOUT_CLEAR_PRESENCE, CLEAR_PRESENCE, [this] {
                this->vehicle_leaving_state = VehicleLeavingState::NO;
            });
        }
        // if the door is closed, clear the presence detect window since a vehicle
        // can't be arriving or leaving with the door shut
        if (*this->door_state == DoorState::CLOSED) {
            this->flags_.presence_detect_window_active = false;
            this->cancel_timeout(TIMEOUT_PRESENCE_DETECT_WINDOW);
        }
    }
}
#endif

Result RATGDOComponent::call_protocol(Args args)
{
    return this->protocol_->call(args);
}

void RATGDOComponent::query_status() { this->protocol_->call(QueryStatus { }); }

void RATGDOComponent::query_openings()
{
    this->protocol_->call(QueryOpenings { });
}

void RATGDOComponent::sync()
{
    this->protocol_->sync();

    // dry contact protocol:
    // needed to trigger the intial state of the limit switch sensors
    // ideally this would be in drycontact::sync
#ifdef PROTOCOL_DRYCONTACT
    this->protocol_->set_open_limit(this->dry_contact_open_sensor_->state);
    this->protocol_->set_close_limit(this->dry_contact_close_sensor_->state);
#endif
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

void RATGDOComponent::door_open()
{
    if (*this->door_state == DoorState::OPENING) {
        return; // gets ignored by opener
    }

    this->door_action(DoorAction::OPEN);

    if (*this->opening_duration > 0) {
        // query state in case we don't get a status message
        this->set_timeout(
            TIMEOUT_DOOR_QUERY_STATE, (*this->opening_duration + 2) * 1000,
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
                this->door_action(DoorAction::CLOSE);
            } else {
                ESP_LOGW(TAG, "Door did not stop, ignoring close command");
            }
        });
        return;
    }

    if (this->flags_.obstruction_sensor_detected) {
        this->door_action(DoorAction::CLOSE);
    } else if (*this->door_state == DoorState::OPEN) {
        ESP_LOGD(TAG, "No obstruction sensors detected. Close using TOGGLE.");
        this->door_action(DoorAction::TOGGLE);
    }

    if (*this->closing_duration > 0) {
        // query state in case we don't get a status message
        this->set_timeout(
            TIMEOUT_DOOR_QUERY_STATE, (*this->closing_duration + 2) * 1000,
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
        ESP_LOGW(TAG, "The door is not moving.");
        return;
    }
    this->door_action(DoorAction::STOP);
}

void RATGDOComponent::door_toggle() { this->door_action(DoorAction::TOGGLE); }

void RATGDOComponent::door_action(DoorAction action)
{
#ifdef RATGDO_USE_CLOSING_DELAY
    if (*this->closing_delay > 0 && (action == DoorAction::CLOSE || (action == DoorAction::TOGGLE && *this->door_state != DoorState::CLOSED))) {
        this->door_action_delayed = DoorActionDelayed::YES;
        this->set_timeout(TIMEOUT_DOOR_ACTION, *this->closing_delay * 1000, [this] {
            this->door_action_delayed = DoorActionDelayed::NO;
            this->protocol_->door_action(DoorAction::CLOSE);
        });
    } else {
        this->protocol_->door_action(action);
    }
#else
    this->protocol_->door_action(action);
#endif
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
    ESP_LOGD(TAG, "Moving to position %.2f in %.1fs", position,
        operation_time / 1000.0);

    this->door_action(delta > 0 ? DoorAction::OPEN : DoorAction::CLOSE);
    this->set_timeout(TIMEOUT_MOVE_TO_POSITION, operation_time,
        [this] { this->door_action(DoorAction::STOP); });
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

void RATGDOComponent::light_toggle()
{
    this->light_state = light_state_toggle(*this->light_state);
    this->protocol_->light_action(LightAction::TOGGLE);
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

void RATGDOComponent::lock_toggle()
{
    this->lock_state = lock_state_toggle(*this->lock_state);
    this->protocol_->lock_action(LockAction::TOGGLE);
}

// Subscribe implementations are now templates in ratgdo.h

// dry contact methods
void RATGDOComponent::set_dry_contact_open_sensor(
    esphome::binary_sensor::BinarySensor* dry_contact_open_sensor)
{
    dry_contact_open_sensor_ = dry_contact_open_sensor;
    dry_contact_open_sensor_->add_on_state_callback([this](bool sensor_value) {
        this->protocol_->set_open_limit(sensor_value);
        this->door_position = 1.0;
    });
}

void RATGDOComponent::set_dry_contact_close_sensor(
    esphome::binary_sensor::BinarySensor* dry_contact_close_sensor)
{
    dry_contact_close_sensor_ = dry_contact_close_sensor;
    dry_contact_close_sensor_->add_on_state_callback([this](bool sensor_value) {
        this->protocol_->set_close_limit(sensor_value);
        this->door_position = 0.0;
    });
}

} // namespace esphome::ratgdo

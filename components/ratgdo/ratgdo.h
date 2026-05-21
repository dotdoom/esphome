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

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/preferences.h"

#include <bitset>
#include <type_traits>
#include <utility>

#include "callbacks.h"
#include "macros.h"
#include "observable.h"
#include "protocol.h"
#include "ratgdo_state.h"

// Observable subscriber counts — set by Python codegen via cg.add_define().
// Missing defines are a build error to catch codegen issues early.
#ifndef RATGDO_MAX_DOOR_STATE_SUBSCRIBERS
#error "RATGDO_MAX_DOOR_STATE_SUBSCRIBERS must be defined by codegen"
#endif
#ifndef RATGDO_MAX_DOOR_ACTION_DELAYED_SUBSCRIBERS
#error "RATGDO_MAX_DOOR_ACTION_DELAYED_SUBSCRIBERS must be defined by codegen"
#endif
#ifndef RATGDO_MAX_DISTANCE_SUBSCRIBERS
#error "RATGDO_MAX_DISTANCE_SUBSCRIBERS must be defined by codegen"
#endif

namespace esphome {
class InternalGPIOPin;
} // namespace esphome

namespace esphome::ratgdo {

class RATGDOComponent;
typedef Parented<RATGDOComponent> RATGDOClient;

const float DOOR_POSITION_UNKNOWN = -1.0;
const float DOOR_DELTA_UNKNOWN = -2.0;

using protocol::Args;
using protocol::Result;

class RATGDOComponent : public Component {
public:
    RATGDOComponent()
    {
    }

    void setup() override;
    void loop() override;
    void dump_config() override;
    void on_shutdown() override;

    void init_protocol();

    float start_opening { -1 };
    single_observable<float> opening_duration { 0 };
    float start_closing { -1 };
    single_observable<float> closing_duration { 0 };

    single_observable<int16_t> target_distance_measurement { -1 };
    std::bitset<256> in_range; // the length of this bitset determines how many out of range readings are required for presence detection to change states
    observable<int16_t, RATGDO_MAX_DISTANCE_SUBSCRIBERS> last_distance_measurement { 0 };

    single_observable<uint16_t> openings { 0 }; // number of times the door has been opened

    observable<DoorState, RATGDO_MAX_DOOR_STATE_SUBSCRIBERS> door_state { DoorState::UNKNOWN };
    observable<float, RATGDO_MAX_DOOR_STATE_SUBSCRIBERS> door_position { DOOR_POSITION_UNKNOWN };
    observable<DoorActionDelayed, RATGDO_MAX_DOOR_ACTION_DELAYED_SUBSCRIBERS> door_action_delayed { DoorActionDelayed::NO };

    unsigned long door_start_moving { 0 };
    float door_start_position { DOOR_POSITION_UNKNOWN };
    float door_move_delta { DOOR_DELTA_UNKNOWN };
    uint16_t position_sync_remaining_ { 0 };

    single_observable<LightState> light_state { LightState::UNKNOWN };
    single_observable<LockState> lock_state { LockState::UNKNOWN };

    OnceCallbacks<void(DoorState)> on_door_state_;

    single_observable<bool> sync_failed { false };

    void set_output_gdo_pin(InternalGPIOPin* pin) { this->output_gdo_pin_ = pin; }
    void set_input_gdo_pin(InternalGPIOPin* pin) { this->input_gdo_pin_ = pin; }

    Result call_protocol(Args args);

    void received(const DoorState door_state);
    void received(const LightState light_state);
    void received(const LockState lock_state);
    void received(const LightAction light_action);
    void received(const Openings openings);

    // door
    void door_toggle();
    void door_open();
    void door_close();
    void door_stop();

    void door_action(DoorAction action);
    void door_move_to_position(float position);
    void set_door_position(float door_position) { this->door_position = door_position; }
    void set_opening_duration(float duration);
    void set_closing_duration(float duration);
    void schedule_door_position_sync(float update_period = 500);
    void door_position_update();
    void cancel_position_sync_callbacks();
    void set_target_distance_measurement(int16_t distance);
    void set_distance_measurement(int16_t distance);

    // light
    void light_on();
    void light_off();
    LightState get_light_state() const;

    // lock
    void lock();
    void unlock();

    // button functionality
    void query_status();
    void query_openings();
    void sync();

    using Component::set_timeout;

    void set_door_state_expiry();
    void cancel_door_state_expiry();

    // Register a one-shot door state callback with automatic expiry.
    //
    // Handles secplus1's nested callback chains where opening from
    // STOPPED requires multiple state transitions:
    //
    //   on_door_state(outer_cb)          // wait for CLOSING
    //     → set_door_state_expiry()      // expiry A
    //     → [door reports CLOSING]
    //       → outer_cb fires, calls:
    //         toggle_door()
    //         on_door_state(inner_cb)    // wait for STOPPED
    //           → set_door_state_expiry() // expiry B (replaces A)
    //           → [door reports STOPPED]
    //             → inner_cb fires
    //               toggle_door()        // door now opening
    //               count()==0 → cancel expiry B
    //
    // The user callback runs BEFORE the expiry check because it may
    // re-arm the chain by calling on_door_state() again. If it does,
    // the new call sets expiry B which replaces expiry A (same timeout
    // ID = replace, not add). We only cancel expiry when count()==0,
    // meaning no new callback was queued — otherwise we'd cancel
    // expiry B here and leave the inner callback without protection.
    template <typename F>
    void on_door_state(F&& callback)
    {
        using Cb = std::decay_t<F>;
        this->on_door_state_([this, cb = Cb(std::forward<F>(callback))](DoorState s) {
            cb(s);
            if (!this->on_door_state_.count()) {
                this->cancel_door_state_expiry();
            }
        });
        this->set_door_state_expiry();
    }

    // children subscriptions — type-safe templates (no std::function)
    // Callbacks must be trivially copyable and fit in Callback storage
    // (3 * sizeof(void*)), e.g. [this] or [this, f] lambdas.
    // Enforced at compile time by Callback::create().
    template <typename F>
    void subscribe_rolling_code_counter(F&& f);
    template <typename F>
    void subscribe_opening_duration(F&& f);
    template <typename F>
    void subscribe_closing_duration(F&& f);
    template <typename F>
    void subscribe_openings(F&& f);
    template <typename F>
    void subscribe_door_state(F&& f);
    template <typename F>
    void subscribe_light_state(F&& f);
    template <typename F>
    void subscribe_lock_state(F&& f);
    template <typename F>
    void subscribe_sync_failed(F&& f);
    template <typename F>
    void subscribe_door_action_delayed(F&& f);
    template <typename F>
    void subscribe_distance_measurement(F&& f);

protected:
    // Pointers first (4-byte aligned)
    protocol::Protocol* protocol_;
    InternalGPIOPin* output_gdo_pin_;
    InternalGPIOPin* input_gdo_pin_;

    // Bool members packed into bitfield
    struct {
        uint8_t reserved : 8; // Reserved for future use
    } flags_ { 0 };

    // Subscriber counters for defer name allocation
    uint8_t door_state_sub_num_ { 0 };
    uint8_t door_action_delayed_sub_num_ { 0 };
    uint8_t distance_sub_num_ { 0 };
}; // RATGDOComponent

void log_subscriber_overflow(const LogString* observable_name, uint32_t max);

inline uint32_t get_scheduler_id(uint32_t base, uint32_t count, uint8_t& counter, const LogString* observable_name)
{
    if (count == 0) {
        log_subscriber_overflow(observable_name, count);
        return base;
    }
    if (counter >= count) {
        log_subscriber_overflow(observable_name, count);
        return base + count - 1; // reuse last ID to avoid collision with first subscriber
    }
    return base + counter++;
}

// Scheduler IDs using uint32_t ranges to avoid heap allocations
// Bases are auto-generated from counts to prevent ID conflicts
namespace scheduler_ids {
    inline constexpr uint32_t INTERVAL_POSITION_SYNC = 0;

    // Multi-subscriber ranges — counts derived from codegen defines
    inline constexpr uint32_t DEFER_DOOR_STATE_COUNT = RATGDO_MAX_DOOR_STATE_SUBSCRIBERS;
    inline constexpr uint32_t DEFER_DOOR_STATE_BASE = INTERVAL_POSITION_SYNC + 1;

    inline constexpr uint32_t DEFER_DOOR_ACTION_DELAYED_COUNT = RATGDO_MAX_DOOR_ACTION_DELAYED_SUBSCRIBERS;
    inline constexpr uint32_t DEFER_DOOR_ACTION_DELAYED_BASE = DEFER_DOOR_STATE_BASE + DEFER_DOOR_STATE_COUNT;

    inline constexpr uint32_t DEFER_DISTANCE_COUNT = RATGDO_MAX_DISTANCE_SUBSCRIBERS;
    inline constexpr uint32_t DEFER_DISTANCE_BASE = DEFER_DOOR_ACTION_DELAYED_BASE + DEFER_DOOR_ACTION_DELAYED_COUNT;
    inline constexpr uint32_t DEFER_DISTANCE_END = DEFER_DISTANCE_BASE + DEFER_DISTANCE_COUNT;

    // Single-subscriber IDs
    enum : uint32_t {
        DEFER_ROLLING_CODE = DEFER_DISTANCE_END,
        DEFER_OPENING_DURATION,
        DEFER_CLOSING_DURATION,
        DEFER_OPENINGS,
        DEFER_LIGHT_STATE,
        DEFER_LOCK_STATE,

        // Named timeout IDs (replacing string-based names)
        TIMEOUT_DOOR_QUERY_STATE,
        TIMEOUT_DOOR_ACTION,
        TIMEOUT_MOVE_TO_POSITION,
        TIMEOUT_DOOR_STATE_EXPIRY,
        TIMEOUT_SYNC,
    };
} // namespace scheduler_ids

// Template implementations for subscribe methods.
// Each wraps the callback in a deferred call so that if the observable
// fires multiple times during one loop iteration, only the last value
// is dispatched to the child component.

template <typename F>
void RATGDOComponent::subscribe_rolling_code_counter(F&& f)
{
    // change update to children is defered until after component loop
    // if multiple changes occur during component loop, only the last one is notified
    auto counter = this->protocol_->call(protocol::GetRollingCodeCounter { });
    if (counter.tag == protocol::Result::Tag::rolling_code_counter) {
        counter.value.rolling_code_counter.value->subscribe([this, f](uint32_t state) {
            defer(scheduler_ids::DEFER_ROLLING_CODE, [f, state] { f(state); });
        });
    }
}

template <typename F>
void RATGDOComponent::subscribe_opening_duration(F&& f)
{
    this->opening_duration.subscribe([this, f](float state) {
        defer(scheduler_ids::DEFER_OPENING_DURATION, [f, state] { f(state); });
    });
}

template <typename F>
void RATGDOComponent::subscribe_closing_duration(F&& f)
{
    this->closing_duration.subscribe([this, f](float state) {
        defer(scheduler_ids::DEFER_CLOSING_DURATION, [f, state] { f(state); });
    });
}

template <typename F>
void RATGDOComponent::subscribe_openings(F&& f)
{
    this->openings.subscribe([this, f](uint16_t state) {
        defer(scheduler_ids::DEFER_OPENINGS, [f, state] { f(state); });
    });
}

template <typename F>
void RATGDOComponent::subscribe_door_state(F&& f)
{
    uint32_t id = get_scheduler_id(scheduler_ids::DEFER_DOOR_STATE_BASE, scheduler_ids::DEFER_DOOR_STATE_COUNT,
        this->door_state_sub_num_, LOG_STR("door_state"));
    this->door_state.subscribe([this, f, id](DoorState state) {
        defer(id, [this, f, state] { f(state, *this->door_position); });
    });
    this->door_position.subscribe([this, f, id](float position) {
        defer(id, [this, f, position] { f(*this->door_state, position); });
    });
}

template <typename F>
void RATGDOComponent::subscribe_light_state(F&& f)
{
    this->light_state.subscribe([this, f](LightState state) {
        defer(scheduler_ids::DEFER_LIGHT_STATE, [f, state] { f(state); });
    });
}

template <typename F>
void RATGDOComponent::subscribe_lock_state(F&& f)
{
    this->lock_state.subscribe([this, f](LockState state) {
        defer(scheduler_ids::DEFER_LOCK_STATE, [f, state] { f(state); });
    });
}

template <typename F>
void RATGDOComponent::subscribe_sync_failed(F&& f)
{
    this->sync_failed.subscribe(std::forward<F>(f));
}

template <typename F>
void RATGDOComponent::subscribe_door_action_delayed(F&& f)
{
    uint32_t id = get_scheduler_id(scheduler_ids::DEFER_DOOR_ACTION_DELAYED_BASE, scheduler_ids::DEFER_DOOR_ACTION_DELAYED_COUNT,
        this->door_action_delayed_sub_num_, LOG_STR("door_action_delayed"));
    this->door_action_delayed.subscribe([this, f, id](DoorActionDelayed state) {
        defer(id, [f, state] { f(state); });
    });
}

template <typename F>
void RATGDOComponent::subscribe_distance_measurement(F&& f)
{
    uint32_t id = get_scheduler_id(scheduler_ids::DEFER_DISTANCE_BASE, scheduler_ids::DEFER_DISTANCE_COUNT,
        this->distance_sub_num_, LOG_STR("distance_measurement"));
    this->last_distance_measurement.subscribe([this, f, id](int16_t state) {
        defer(id, [f, state] { f(state); });
    });
}

} // namespace esphome::ratgdo

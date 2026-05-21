#pragma once

#include "common.h"
#include "ratgdo_state.h"

namespace esphome {

class Scheduler;
class InternalGPIOPin;

} // namespace esphome

namespace esphome::ratgdo {

class RATGDOComponent;

namespace protocol {

    struct SetRollingCodeCounter {
        uint32_t counter;
    };
    struct GetRollingCodeCounter {
    };
    struct SetClientID {
        uint64_t client_id;
    };
    struct QueryStatus {
    };
    struct QueryOpenings {
    };

    // a poor man's sum-type, because C++
    SUM_TYPE(Args,
        (SetRollingCodeCounter, set_rolling_code_counter),
        (GetRollingCodeCounter, get_rolling_code_counter),
        (SetClientID, set_client_id),
        (QueryStatus, query_status),
        (QueryOpenings, query_openings), )

    struct RollingCodeCounter {
        single_observable<uint32_t>* value;
    };

    SUM_TYPE(Result,
        (RollingCodeCounter, rolling_code_counter), )

    class Protocol {
    public:
        virtual void setup(RATGDOComponent* ratgdo, Scheduler* scheduler, InternalGPIOPin* rx_pin, InternalGPIOPin* tx_pin);
        virtual void loop();
        virtual void dump_config();

        virtual void on_shutdown() { }

        virtual void sync();

        virtual void light_action(LightAction action);
        virtual void lock_action(LockAction action);
        virtual void door_action(DoorAction action);

        virtual protocol::Result call(protocol::Args args);
    };

}
} // namespace esphome::ratgdo

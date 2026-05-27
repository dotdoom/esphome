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

        virtual void query_status() {}
        virtual void query_openings() {}
    };

}
} // namespace esphome::ratgdo

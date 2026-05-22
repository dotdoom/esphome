#pragma once

#include "esphome/core/optional.h"
#include "esphome/core/preferences.h"
#include "ratgdo_uart.h"

#include "callbacks.h"
#include "common.h"
#include "observable.h"
#include "protocol.h"
#include "ratgdo_state.h"

namespace esphome {

class Scheduler;
class InternalGPIOPin;

} // namespace esphome

namespace esphome::ratgdo {
class RATGDOComponent;

namespace secplus2 {

    using namespace esphome::ratgdo::protocol;

    static const uint8_t PACKET_LENGTH = 19;
    typedef uint8_t WirePacket[PACKET_LENGTH];

    ENUM_SPARSE(CommandType, uint16_t,
        (UNKNOWN, 0x000),
        (GET_STATUS, 0x080),
        (STATUS, 0x081),

        (LOCK, 0x18c),
        (DOOR_ACTION, 0x280),
        (LIGHT, 0x281),

        (GET_OPENINGS, 0x48b),
        (OPENINGS, 0x48c), // openings = (byte1<<8)+byte2
    )

    inline bool operator==(const uint16_t cmd_i, const CommandType& cmd_e) { return cmd_i == static_cast<uint16_t>(cmd_e); }
    inline bool operator==(const CommandType& cmd_e, const uint16_t cmd_i) { return cmd_i == static_cast<uint16_t>(cmd_e); }

    enum class IncrementRollingCode {
        NO,
        YES,
    };

    struct Command {
        CommandType type;
        uint8_t nibble;
        uint8_t byte1;
        uint8_t byte2;

        Command()
            : type(CommandType::UNKNOWN)
        {
        }
        Command(CommandType type_, uint8_t nibble_ = 0, uint8_t byte1_ = 0, uint8_t byte2_ = 0)
            : type(type_)
            , nibble(nibble_)
            , byte1(byte1_)
            , byte2(byte2_)
        {
        }
    };

    class Secplus2 : public Protocol {
    public:
        void setup(RATGDOComponent* ratgdo, Scheduler* scheduler, InternalGPIOPin* rx_pin, InternalGPIOPin* tx_pin);
        void loop();
        void dump_config();
        void on_shutdown() override;

        void sync();

        void light_action(LightAction action);
        void lock_action(LockAction action);
        void door_action(DoorAction action);

        Result call(Args args);

    protected:
        void increment_rolling_code_counter(int delta = 1);
        void set_rolling_code_counter(uint32_t counter);
        void set_client_id(uint64_t client_id);

        optional<Command> read_command();
        void handle_command(const Command& cmd);

        void send_command(Command cmd, IncrementRollingCode increment = IncrementRollingCode::YES);
        template <typename F>
        void send_command(Command cmd, IncrementRollingCode increment, F&& on_sent)
        {
            // Only register the callback if the command will be accepted.
            // If transmit_pending is set the command will be dropped, and
            // a stale callback would fire when the previous pending packet
            // transmits -- executing logic (e.g. the second phase of a
            // door_command) at the wrong time.
            //
            // Register before send_command() because transmit_packet() may
            // succeed immediately and call on_command_sent_.trigger() inline.
            if (this->flags_.transmit_pending) {
                return;
            }
            this->on_command_sent_(std::forward<F>(on_sent));
            this->send_command(cmd, increment);
        }
        void encode_packet(Command cmd, WirePacket& packet);
        bool transmit_packet();

        void door_command(DoorAction action);

        void query_status();
        void query_openings();

        void print_packet(const esphome::LogString* prefix, const WirePacket& packet) const;
        optional<Command> decode_packet(const WirePacket& packet) const;

        void sync_helper(uint32_t start, uint32_t delay, uint8_t tries);

        // 8-byte member first (may require 8-byte alignment on some 32-bit systems)
        uint64_t client_id_ { 0x539 };

        // Pointers (4-byte aligned)
        InternalGPIOPin* tx_pin_;
        InternalGPIOPin* rx_pin_;
        RATGDOComponent* ratgdo_;
        Scheduler* scheduler_;

        // 4-byte members
        uint32_t transmit_pending_start_ { 0 };
        uint32_t rx_msg_start_ { 0 };
        uint32_t rx_last_read_ { 0 };

        // Larger structures
        single_observable<uint32_t> rolling_code_counter_ { 0 };
        ESPPreferenceObject rolling_code_pref_;
        OnceCallbacks<void()> on_command_sent_;
        RatgdoUART uart_;

        // 19-byte arrays
        WirePacket tx_packet_;
        WirePacket rx_packet_;

        // Small members at the end
        uint16_t rx_byte_count_ { 0 };
        struct {
            uint8_t transmit_pending : 1;
            uint8_t rx_reading_msg : 1;
        } flags_ { 0 };
    };
} // namespace secplus2
} // namespace esphome::ratgdo

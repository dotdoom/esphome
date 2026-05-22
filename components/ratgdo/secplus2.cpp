
#include "secplus2.h"
#include "ratgdo.h"

#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/scheduler.h"

extern "C" {
#include "secplus.h"
}

namespace esphome::ratgdo {
namespace secplus2 {

    using namespace scheduler_ids;

    // MAX_CODES_WITHOUT_FLASH_WRITE is a bit of a guess
    // since we write the flash at most every every 1min
    //
    // We want the rolling counter to be high enough that the
    // GDO will accept the command after an unexpected reboot
    // that did not save the counter to flash in time which
    // results in the rolling counter being behind what the GDO
    // expects.
    static const uint8_t MAX_CODES_WITHOUT_FLASH_WRITE = 60;

    static const char* const TAG = "ratgdo_secplus2";

    void Secplus2::setup(RATGDOComponent* ratgdo, Scheduler* scheduler, InternalGPIOPin* rx_pin, InternalGPIOPin* tx_pin)
    {
        this->ratgdo_ = ratgdo;
        this->scheduler_ = scheduler;
        this->tx_pin_ = tx_pin;
        this->rx_pin_ = rx_pin;

        this->uart_.begin(9600, RATGDO_UART_8N1, rx_pin->get_pin(), tx_pin->get_pin(), true);
        this->uart_.enableIntTx(false);
        this->uart_.enableAutoBaud(true);
    }

    void Secplus2::loop()
    {
        if (this->flags_.transmit_pending) {
            if (!this->transmit_packet()) {
                return;
            }
        }

        auto cmd = this->read_command();
        if (cmd) {
            this->handle_command(*cmd);
        }
    }

    void Secplus2::dump_config()
    {
        ESP_LOGCONFIG(TAG, "  Rolling Code Counter: %d", *this->rolling_code_counter_);
        ESP_LOGCONFIG(TAG, "  Client ID: %d", this->client_id_);
        ESP_LOGCONFIG(TAG, "  Protocol: SEC+ v2");
    }

    void Secplus2::on_shutdown()
    {
        this->uart_.on_shutdown();
    }

    void Secplus2::sync_helper(uint32_t start, uint32_t delay, uint8_t tries)
    {
        if (*this->ratgdo_->door_state == DoorState::UNKNOWN) {
            ESP_LOGD(TAG, "Sync: querying status (attempt %d)...", tries);
            this->query_status();
        } else if (*this->ratgdo_->openings == 0) {
            ESP_LOGD(TAG, "Sync: querying openings (attempt %d)...", tries);
            this->query_openings();
        } else {
            ESP_LOGD(TAG, "Sync successful!");
            return;
        }

        if (tries == 10 && *this->ratgdo_->door_state == DoorState::UNKNOWN) {
            // After 10 failed attempts to even get status, try jumping the rolling code.
            // This handles cases where the device rolling code is way behind the GDO.
            ESP_LOGW(TAG, "Sync: jumping rolling code counter...");
            this->increment_rolling_code_counter(MAX_CODES_WITHOUT_FLASH_WRITE);
        }

        // not sync-ed after 30s, notify failure
        if (millis() - start > 30000) {
            ESP_LOGW(TAG, "Triggering sync failed actions.");
            this->ratgdo_->sync_failed = true;
        } else {
            // Use a slightly longer delay between queries during sync to avoid bus saturation
            uint32_t next_delay = (tries < 5) ? 1000 : 2000;
            this->scheduler_->set_timeout(this->ratgdo_, TIMEOUT_SYNC, next_delay, [this, start, next_delay, tries]() {
                this->sync_helper(start, next_delay, tries + 1);
            });
        };
    }

    void Secplus2::sync()
    {
        ESP_LOGD(TAG, "Starting sync...");
        this->ratgdo_->sync_failed = false;
        this->scheduler_->cancel_timeout(this->ratgdo_, TIMEOUT_SYNC);
        this->sync_helper(millis(), 500, 0);
    }

    void Secplus2::light_action(LightAction action)
    {
        if (action == LightAction::UNKNOWN) {
            return;
        }
        this->send_command(Command(CommandType::LIGHT, static_cast<uint8_t>(action)));
    }

    void Secplus2::lock_action(LockAction action)
    {
        if (action == LockAction::UNKNOWN) {
            return;
        }
        this->send_command(Command(CommandType::LOCK, static_cast<uint8_t>(action)));
    }

    void Secplus2::door_action(DoorAction action)
    {
        if (action == DoorAction::UNKNOWN) {
            return;
        }
        this->door_command(action);
    }

    Result Secplus2::call(Args args)
    {
        using Tag = Args::Tag;
        if (args.tag == Tag::query_status) {
            this->send_command(CommandType::GET_STATUS);
        } else if (args.tag == Tag::query_openings) {
            this->send_command(CommandType::GET_OPENINGS);
        } else if (args.tag == Tag::get_rolling_code_counter) {
            return Result(RollingCodeCounter { std::addressof(this->rolling_code_counter_) });
        } else if (args.tag == Tag::set_rolling_code_counter) {
            this->set_rolling_code_counter(args.value.set_rolling_code_counter.counter);
        } else if (args.tag == Tag::set_client_id) {
            this->set_client_id(args.value.set_client_id.client_id);
        }
        return { };
    }

    void Secplus2::door_command(DoorAction action)
    {
        this->send_command(Command(CommandType::DOOR_ACTION, static_cast<uint8_t>(action), 1, 1), IncrementRollingCode::NO, [this, action]() {
            this->ratgdo_->set_timeout(150, [this, action] {
                this->send_command(Command(CommandType::DOOR_ACTION, static_cast<uint8_t>(action), 0, 1));
            });
        });
    }

    void Secplus2::query_status()
    {
        this->send_command(CommandType::GET_STATUS);
    }

    void Secplus2::query_openings()
    {
        this->send_command(CommandType::GET_OPENINGS);
    }

    optional<Command> Secplus2::read_command()
    {
        if (!this->flags_.rx_reading_msg) {
            while (this->uart_.available()) {
                uint8_t ser_byte = this->uart_.read();
                this->rx_last_read_ = millis();

                if (ser_byte != 0x55 && ser_byte != 0x01 && ser_byte != 0x00) {
                    {
                        char hex[format_hex_pretty_size(1)];
                        ESP_LOG2(TAG, "Ignoring byte (%d): %s, baud: %d", this->rx_byte_count_, format_hex_pretty_to(hex, &ser_byte, 1), this->uart_.baudRate());
                    }
                    this->rx_byte_count_ = 0;
                    continue;
                }
                this->rx_msg_start_ = ((this->rx_msg_start_ << 8) | ser_byte) & 0xffffff;
                this->rx_byte_count_++;

                // if we are at the start of a message, capture the next 16 bytes
                if (this->rx_msg_start_ == 0x550100) {
                    ESP_LOG1(TAG, "Baud: %d", this->uart_.baudRate());
                    this->rx_packet_[0] = 0x55;
                    this->rx_packet_[1] = 0x01;
                    this->rx_packet_[2] = 0x00;
                    this->rx_byte_count_ = 3;

                    this->flags_.rx_reading_msg = true;
                    break;
                }
            }
        }
        if (this->flags_.rx_reading_msg) {
            while (this->uart_.available()) {
                uint8_t ser_byte = this->uart_.read();
                this->rx_last_read_ = millis();
                this->rx_packet_[this->rx_byte_count_] = ser_byte;
                this->rx_byte_count_++;
                // ESP_LOG2(TAG, "Received byte (%d): %02X, baud: %d", this->rx_byte_count_, ser_byte, this->uart_.baudRate());

                if (this->rx_byte_count_ == PACKET_LENGTH) {
                    this->flags_.rx_reading_msg = false;
                    this->rx_byte_count_ = 0;
                    this->print_packet(LOG_STR("Received packet"), this->rx_packet_);
                    return this->decode_packet(this->rx_packet_);
                }
            }

            if (millis() - this->rx_last_read_ > 100) {
                // if we have a partial packet and it's been over 100ms since last byte was read,
                // the rest is not coming (a full packet should be received in ~20ms),
                // discard it so we can read the following packet correctly
                ESP_LOGW(TAG, "Discard incomplete packet, length: %d", this->rx_byte_count_);
                this->flags_.rx_reading_msg = false;
                this->rx_byte_count_ = 0;
            }
        }

        return { };
    }

    void Secplus2::print_packet(const esphome::LogString* prefix, const WirePacket& packet) const
    {
        constexpr size_t hex_size = format_hex_pretty_size(PACKET_LENGTH);
        char hex_buf[hex_size];
        ESP_LOGD(TAG, "%s: [%s]", LOG_STR_ARG(prefix), format_hex_pretty_to(hex_buf, packet, PACKET_LENGTH));
    }

    optional<Command> Secplus2::decode_packet(const WirePacket& packet) const
    {
        uint32_t rolling = 0;
        uint64_t fixed = 0;
        uint32_t data = 0;

        int err = decode_wireline(packet, &rolling, &fixed, &data);
        if (err < 0) {
            ESP_LOGW(TAG, "Decode failed (parity error or invalid frame)");
            return { };
        }

        uint16_t cmd = ((fixed >> 24) & 0xf00) | (data & 0xff);
        data &= ~0xf000; // clear parity nibble

        if ((fixed & 0xFFFFFFFF) == this->client_id_) { // my commands
            ESP_LOGD(TAG, "  mine: rolling=%07" PRIx32 " fixed=%010" PRIx64 " data=%08" PRIx32, rolling, fixed, data);
            return { };
        } else {
            ESP_LOGD(TAG, "  rolling=%07" PRIx32 " fixed=%010" PRIx64 " data=%08" PRIx32, rolling, fixed, data);
        }

        CommandType cmd_type = to_CommandType(cmd, CommandType::UNKNOWN);
        uint8_t nibble = (data >> 8) & 0xff;
        uint8_t byte1 = (data >> 16) & 0xff;
        uint8_t byte2 = (data >> 24) & 0xff;

        ESP_LOGD(TAG, "  cmd=%03x (%s) byte2=%02x byte1=%02x nibble=%01x", cmd, LOG_STR_ARG(CommandType_to_string(cmd_type)), byte2, byte1, nibble);

        return Command { cmd_type, nibble, byte1, byte2 };
    }

    void Secplus2::handle_command(const Command& cmd)
    {
        ESP_LOGD(TAG, "Handle command: %s (nibble=%01x byte1=%02x byte2=%02x)", LOG_STR_ARG(CommandType_to_string(cmd.type)), cmd.nibble, cmd.byte1, cmd.byte2);

        if (cmd.type == CommandType::STATUS) {

            this->ratgdo_->received(to_DoorState(cmd.nibble, DoorState::UNKNOWN));
            this->ratgdo_->received(to_LightState((cmd.byte2 >> 1) & 1, LightState::UNKNOWN));
            this->ratgdo_->received(to_LockState((cmd.byte2 & 1), LockState::UNKNOWN));
        } else if (cmd.type == CommandType::LIGHT) {
            this->ratgdo_->received(to_LightAction(cmd.nibble, LightAction::UNKNOWN));
        } else if (cmd.type == CommandType::OPENINGS) {
            this->ratgdo_->received(Openings { static_cast<uint16_t>((cmd.byte1 << 8) | cmd.byte2), cmd.nibble });
        }
    }

    void Secplus2::send_command(Command command, IncrementRollingCode increment)
    {
        {
            uint8_t data[] = { command.byte2, command.byte1, command.nibble };
            constexpr size_t hex_size = format_hex_pretty_size(3);
            char hex[hex_size];
            ESP_LOGD(TAG, "Send command: %s, data: %s", LOG_STR_ARG(CommandType_to_string(command.type)), format_hex_pretty_to(hex, data, 3));
        }
        if (!this->flags_.transmit_pending) { // have an untransmitted packet
            this->encode_packet(command, this->tx_packet_);
            if (increment == IncrementRollingCode::YES) {
                this->increment_rolling_code_counter();
            }
        } else {
            // unlikely this would happed (unless not connected to GDO), we're ensuring any pending packet
            // is transmitted each loop before doing anyting else
            if (this->transmit_pending_start_ > 0) {
                ESP_LOGW(TAG, "Have untransmitted packet, ignoring command: %s", LOG_STR_ARG(CommandType_to_string(command.type)));
            } else {
                ESP_LOGW(TAG, "Not connected to GDO, ignoring command: %s", LOG_STR_ARG(CommandType_to_string(command.type)));
            }
        }
        this->transmit_packet();
    }

    void Secplus2::encode_packet(Command command, WirePacket& packet)
    {
        auto cmd = static_cast<uint64_t>(command.type);
        uint64_t fixed = ((cmd & ~0xff) << 24) | this->client_id_;
        uint32_t data = (static_cast<uint64_t>(command.byte2) << 24) | (static_cast<uint64_t>(command.byte1) << 16) | (static_cast<uint64_t>(command.nibble) << 8) | (cmd & 0xff);

        ESP_LOGD(TAG, "  transmit: rolling=%07" PRIx32 " fixed=%010" PRIx64 " data=%08" PRIx32, *this->rolling_code_counter_, fixed, data);
        encode_wireline(*this->rolling_code_counter_, fixed, data, packet);
    }

    bool Secplus2::transmit_packet()
    {
        auto now = micros();

        while (micros() - now < 1300) {
            if (this->rx_pin_->digital_read()) {
                if (!this->flags_.transmit_pending) {
                    this->flags_.transmit_pending = true;
                    this->transmit_pending_start_ = millis();
                    ESP_LOGD(TAG, "Collision detected, waiting to send packet");
                } else if (millis() - this->transmit_pending_start_ >= 5000) {
                    this->transmit_pending_start_ = 0; // to indicate GDO not connected state
                }
                return false;
            }
            delayMicroseconds(100);
        }

        this->print_packet(LOG_STR("Sending packet"), this->tx_packet_);

        this->uart_.transmit_secplus2_preamble();
        this->uart_.write(this->tx_packet_, PACKET_LENGTH);

        this->flags_.transmit_pending = false;
        this->transmit_pending_start_ = 0;
        this->on_command_sent_.trigger();
        return true;
    }

    void Secplus2::increment_rolling_code_counter(int delta)
    {
        this->rolling_code_counter_ = (*this->rolling_code_counter_ + delta) & 0xfffffff;
    }

    void Secplus2::set_rolling_code_counter(uint32_t counter)
    {
        ESP_LOGV(TAG, "Set rolling code counter to %d", counter);
        this->rolling_code_counter_ = counter;
    }

    void Secplus2::set_client_id(uint64_t client_id)
    {
        this->client_id_ = client_id & 0xFFFFFFFF;
    }

} // namespace secplus2
} // namespace esphome::ratgdo

#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NETWORK

/* Potential future improvements:

 - hostname instead of IP address
 - custom mapping of log leves to syslog levels
 - set custom facility
 - protocol support: Structured Data, BSD, TCP
 - allow specifying time component to get timestamp
 - code optimization (e.g. marking methods as HOT or using fewer complex ops)
 - autoretries (fixes esp8266 skipping logs at startup due to dump_config flood)
 */

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/logger/logger.h"

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || \
    defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
#include "esphome/components/socket/socket.h"
#else
#include "WiFiUdp.h"
#endif

namespace esphome {

namespace syslog {

class Syslog : public Component, public logger::LogListener {
 public:
  explicit Syslog();

  float get_setup_priority() const override {
    return setup_priority::AFTER_WIFI;
  }

  void setup() override;
  void loop() override;
  void dump_config() override;

  void set_server_ip_address(const std::string &address) {
    this->server_ip_address_ = address;
  }
  void set_server_port(uint16_t port) { this->server_port_ = port; }
  void set_hostname(const std::string &hostname) { this->hostname_ = hostname; }
  void set_min_log_level(int log_level) { this->min_log_level_ = log_level; }
  void set_forward_logger(bool forward) { this->forward_logger_ = forward; }
  void set_strip_color_codes(bool strip) { this->strip_color_codes_ = strip; }

  void on_log(uint8_t, const char *, const char *, size_t) override;
  void log(int level, const std::string &tag, const std::string &msg);

 protected:
  std::string server_ip_address_;
  uint16_t server_port_;
  std::string hostname_;
  int min_log_level_;
  bool forward_logger_;
  bool strip_color_codes_;

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || \
    defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  std::unique_ptr<esphome::socket::Socket> socket_{};
  struct sockaddr_storage destination_;
  socklen_t destination_len_;
#else
  // The socket class doesn't implement UDP for ESP8266.
  WiFiUDP udp_client_{};
#endif

  int errors_encountered_;
  std::string latest_error_message_;

  template <typename... Ts>
  class SyslogLogAction : public Action<Ts...> {
   public:
    SyslogLogAction(Syslog *parent) : parent_(parent) {}
    TEMPLATABLE_VALUE(int, level)
    TEMPLATABLE_VALUE(std::string, tag)
    TEMPLATABLE_VALUE(std::string, payload)

    void play(Ts... x) override {
      this->parent_->log(this->level_.value(x...), this->tag_.value(x...),
                         this->payload_.value(x...));
    }

   protected:
    Syslog *parent_;
  };
};

}  // namespace syslog

}  // namespace esphome
#endif

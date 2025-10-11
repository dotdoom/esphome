#include "syslog.h"
#ifdef USE_NETWORK

#include <sstream>

#include "esphome/components/network/util.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {

namespace syslog {

std::string remove_ansi_colors(const std::string &input) {
  std::string result;
  bool escape_sequence = false;

  for (char c : input) {
    if (c == '\033') {  // Start of an escape sequence
      escape_sequence = true;
    } else if (escape_sequence && c == 'm') {  // End of an escape sequence
      escape_sequence = false;
    } else if (!escape_sequence) {  // Normal character
      result += c;
    }
  }

  return result;
}

enum Priority {
  EMERG = 0,   /* System is unusable */
  ALERT = 1,   /* Action must be taken immediately */
  CRIT = 2,    /* Critical conditions */
  ERR = 3,     /* Error conditions */
  WARNING = 4, /* Warning conditions */
  NOTICE = 5,  /* Normal but significant conditions */
  INFO = 6,    /* Informational messages */
  DEBUG = 7,   /* Debug-level messages */
};

static const uint8_t esphome_to_syslog_log_levels[] = {
    /* ESPHOME_LOG_LEVEL_NONE */ Priority::DEBUG,
    /* ESPHOME_LOG_LEVEL_ERROR */ Priority::ERR,
    /* ESPHOME_LOG_LEVEL_WARN */ Priority::WARNING,
    /* ESPHOME_LOG_LEVEL_INFO */ Priority::INFO,
    /* ESPHOME_LOG_LEVEL_CONFIG */ Priority::NOTICE,
    /* ESPHOME_LOG_LEVEL_DEBUG */ Priority::DEBUG,
    /* ESPHOME_LOG_LEVEL_VERBOSE */ Priority::DEBUG,
    /* ESPHOME_LOG_LEVEL_VERY_VERBOSE */ Priority::DEBUG,
};

static const uint8_t ESPHOME_LOG_LEVELS = 8;

static const char *TAG = "syslog";

Syslog::Syslog() {
  this->hostname_ = ::esphome::network::get_use_address();
  this->errors_encountered_ = 0;
}

void Syslog::setup() {
#ifdef USE_LOGGER
  if (logger::global_logger != nullptr && this->forward_logger_) {
    logger::global_logger->add_on_log_callback(
        [this](int level, const char *tag, const char *raw_message, size_t raw_message_len) {
          if (level > this->min_log_level_) return;

          std::string message = std::string(raw_message, raw_message_len);
          if (this->strip_color_codes_) {
            std::string clean_message = remove_ansi_colors(message);
            this->log(level, tag, clean_message);
          } else {
            this->log(level, tag, message);
          }
        });
  }
#endif

#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || \
    defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  this->socket_ = socket::socket(AF_INET, SOCK_DGRAM, PF_INET);
  // We don't expect any reply from syslog server, so there's no need to block.
  this->socket_->setblocking(false);
  this->destination_len_ = socket::set_sockaddr(
      reinterpret_cast<sockaddr *>(&this->destination_),
      sizeof(this->destination_), this->server_ip_address_, this->server_port_);

  if (this->destination_len_ == 0) {
    std::string error_message(strerror(errno));
    ESP_LOGE(TAG,
             "Cannot use IP address '%s' or port %d for server connection: %s",
             this->server_ip_address_.c_str(), this->server_port_,
             error_message.c_str());
  }
#endif
}

void Syslog::loop() {
  if (!this->latest_error_message_.empty() && this->errors_encountered_ >= 10) {
    ESP_LOGW(TAG, "Failed to send log: %s", latest_error_message_.c_str());
    this->errors_encountered_ = 0;
    latest_error_message_.clear();
  }
}

void Syslog::dump_config() {
  ESP_LOGCONFIG(TAG, "Syslog:");
  ESP_LOGCONFIG(TAG, "  Server IP address: %s",
                this->server_ip_address_.c_str());
  ESP_LOGCONFIG(TAG, "  Server port: %d", this->server_port_);
  ESP_LOGCONFIG(TAG, "  Client ID (syslog hostname): %s",
                this->hostname_.c_str());
#ifdef USE_LOGGER
  ESP_LOGCONFIG(TAG, "  Min log level: %d", this->min_log_level_);
  ESP_LOGCONFIG(TAG, "  Forward logger messages: %s",
                this->forward_logger_ ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Strip color codes: %s",
                this->strip_color_codes_ ? "yes" : "no");
#endif
}

void Syslog::log(int level, const std::string &tag, const std::string &msg) {
  if (level >= ESPHOME_LOG_LEVELS) {
    level = ESPHOME_LOG_LEVELS - 1;
  } else if (level < 0) {
    level = 0;
  }

  std::stringstream payload_stream;
  // rsyslog compatible protocol:
  // <PRI>VERSION SP TIMESTAMP SP HOSTNAME SP APP-NAME SP PROCID SP MSGID SP
  // [SD-ID]s SP MSG
  payload_stream << "<" << static_cast<int>(esphome_to_syslog_log_levels[level])
                 << ">1 - " << this->hostname_ << " " << App.get_name()
                 << " - - - " << msg;

  std::string payload = payload_stream.str();
  ssize_t bytes_sent = 0;
#if defined(USE_SOCKET_IMPL_BSD_SOCKETS) || \
    defined(USE_SOCKET_IMPL_LWIP_SOCKETS)
  if (this->destination_len_ > 0) {
    bytes_sent =
        this->socket_->sendto(payload.c_str(), payload.length(), 0,
                              reinterpret_cast<sockaddr *>(&this->destination_),
                              this->destination_len_);
  }
#else
  if (this->udp_client_.beginPacket(this->server_ip_address_.c_str(),
                                    this->server_port_)) {
    bytes_sent = this->udp_client_.write(payload.c_str(), payload.length());
    if (!this->udp_client_.endPacket()) {
      bytes_sent = 0;
    }
  }
#endif

  if (bytes_sent != payload.length()) {
    // Can't log here as we could be within logger callback, but our loop() will
    // pick it up.
    this->latest_error_message_ = strerror(errno);
    ++this->errors_encountered_;
  }
}

}  // namespace syslog
}  // namespace esphome
#endif

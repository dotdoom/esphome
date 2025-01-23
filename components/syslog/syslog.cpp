#include "syslog.h"

#include <sstream>

#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

#include "esphome/components/network/util.h"
/*
#include "esphome/core/helpers.h"
#include "esphome/core/defines.h"
#include "esphome/core/version.h"
*/

namespace esphome {

namespace syslog {

std::string remove_ansi_colors(const std::string& input) {
  std::string result;
  bool escape_sequence = false;

  for (char c : input) {
    if (c == '\033') { // Start of an escape sequence
      escape_sequence = true;
    } else if (escape_sequence && c == 'm') { // End of an escape sequence
      escape_sequence = false;
    } else if (!escape_sequence) { // Normal character
      result += c;
    }
  }

  return result;
}

enum Priority {
    EMERG   = 0, /* System is unusable */
    ALERT   = 1, /* Action must be taken immediately */
    CRIT    = 2, /* Critical conditions */
    ERR     = 3, /* Error conditions */
    WARNING = 4, /* Warning conditions */
    NOTICE  = 5, /* Normal but significant conditions */
    INFO    = 6, /* Informational messages */
    DEBUG   = 7, /* Debug-level messages */
};

static const uint8_t esphome_to_syslog_log_levels[] = {
    /* ESPHOME_LOG_LEVEL_NONE */         Priority::DEBUG,
    /* ESPHOME_LOG_LEVEL_ERROR */        Priority::ERR,
    /* ESPHOME_LOG_LEVEL_WARN */         Priority::WARNING,
    /* ESPHOME_LOG_LEVEL_INFO */         Priority::INFO,
    /* ESPHOME_LOG_LEVEL_CONFIG */       Priority::NOTICE,
    /* ESPHOME_LOG_LEVEL_DEBUG */        Priority::DEBUG,
    /* ESPHOME_LOG_LEVEL_VERBOSE */      Priority::DEBUG,
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
      [this](int level, const char *tag, const char *message) {
        if (level > this->min_log_level_)
          return;

        if (this->strip_color_codes_) {
          std::string clean_message = remove_ansi_colors(message);
          this->log(level, tag, clean_message);
        } else {
          this->log(level, tag, message);
        }
      });
    }
#endif

  this->socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);

  memset(&this->destination_, 0, sizeof(this->destination_));
  this->destination_.sin_family = AF_INET;
  this->destination_.sin_port = htons(this->server_port_);
  this->destination_.sin_addr.s_addr = inet_addr(this->server_ip_address_.c_str());
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
  ESP_LOGCONFIG(TAG, "  Server IP address: %s", this->server_ip_address_.c_str());
  ESP_LOGCONFIG(TAG, "  Server port: %d", this->server_port_);
  ESP_LOGCONFIG(TAG, "  Client ID (syslog hostname): %s", this->hostname_.c_str());
#ifdef USE_LOGGER
  ESP_LOGCONFIG(TAG, "  Min log level: %d", this->min_log_level_);
  ESP_LOGCONFIG(TAG, "  Forward logger messages: %s", this->forward_logger_ ? "yes" : "no");
  ESP_LOGCONFIG(TAG, "  Strip color codes: %s", this->strip_color_codes_ ? "yes" : "no");
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
  // <PRI>VERSION SP TIMESTAMP SP HOSTNAME SP APP-NAME SP PROCID SP MSGID SP [SD-ID]s SP MSG
  payload_stream
    << "<" << static_cast<int>(esphome_to_syslog_log_levels[level]) << ">1 - "
    << this->hostname_ << " "
    << App.get_name() << " - - - " << msg;

  std::string payload = payload_stream.str();
  ssize_t bytes_sent = ::sendto(
    this->socket_fd_,
    payload.c_str(), payload.length(),
    0,
    reinterpret_cast<sockaddr*>(&this->destination_), sizeof(this->destination_));

  if (bytes_sent != payload.length()) {
    // Can't log here, but loop() will pick it up.
    this->latest_error_message_ = strerror(errno);
    ++this->errors_encountered_;
  }
}

}  // namespace syslog
}  // namespace esphome

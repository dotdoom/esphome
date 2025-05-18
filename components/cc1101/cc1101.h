#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/spi/spi.h"
#include "esphome/components/remote_base/rc_switch_protocol.h"
#include "esphome/components/voltage_sampler/voltage_sampler.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/switch/switch.h"
#include "esphome/components/select/select.h"
#include "esphome/components/text/text.h"
#include <string>
#include "cc1101defs.h"
#include "cc1101sub.h"

namespace esphome {
namespace cc1101 {

class CC1101Component : public PollingComponent,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                              spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_1KHZ> {
 public:
  CC1101Component();

  void set_config_gdo0_pin(InternalGPIOPin *pin) { gdo0_ = pin; }
  void set_config_gdo0_adc_pin(voltage_sampler::VoltageSampler *pin) { gdo0_adc_ = pin; }

  void setup() override;
  void dump_config() override;
  void update() override;
  void loop() override;

  void begin_tx();
  void end_tx();

  CC1101_SUB_NUMBER(output_power, float)
  CC1101_SUB_SELECT(rx_attenuation, RxAttenuation)
  CC1101_SUB_SWITCH(dc_blocking_filter)
  // TODO: CC1101_SUB_SWITCH(manchester)
  CC1101_SUB_NUMBER(tuner_frequency, float)
  CC1101_SUB_NUMBER(tuner_if_frequency, float)
  CC1101_SUB_NUMBER(tuner_bandwidth, float)
  CC1101_SUB_NUMBER(tuner_channel, uint8_t)
  CC1101_SUB_NUMBER(tuner_channel_spacing, float)
  CC1101_SUB_NUMBER(tuner_fsk_deviation, float)
  CC1101_SUB_NUMBER(tuner_msk_deviation, uint8_t)
  CC1101_SUB_NUMBER(tuner_symbol_rate, float)
  CC1101_SUB_SELECT(tuner_sync_mode, SyncMode)
  CC1101_SUB_SWITCH(tuner_carrier_sense_above_threshold)
  CC1101_SUB_SELECT(tuner_modulation, Modulation)
  CC1101_SUB_SELECT(agc_magn_target, MagnTarget)
  CC1101_SUB_SELECT(agc_max_lna_gain, MaxLnaGain)
  CC1101_SUB_SELECT(agc_max_dvga_gain, MaxDvgaGain)
  CC1101_SUB_NUMBER(agc_carrier_sense_abs_thr, int8_t)
  CC1101_SUB_SELECT(agc_carrier_sense_rel_thr, CarrierSenseRelThr)
  CC1101_SUB_SWITCH(agc_lna_priority)
  CC1101_SUB_SELECT(agc_filter_length_fsk_msk, FilterLengthFskMsk)
  CC1101_SUB_SELECT(agc_filter_length_ask_ook, FilterLengthAskOok)
  CC1101_SUB_SELECT(agc_freeze, Freeze)
  CC1101_SUB_SELECT(agc_wait_time, WaitTime)
  CC1101_SUB_SELECT(agc_hyst_level, HystLevel)
  CC1101_SUB_TEXT_SENSOR(chip_id)
  // TODO: PARTNUM
  // TODO: VERSION
  CC1101_SUB_SENSOR(rssi)
  CC1101_SUB_SENSOR(lqi)
  // TODO: CRC OK
  CC1101_SUB_SENSOR(temperature)

 protected:
  InternalGPIOPin *gdo0_;
  voltage_sampler::VoltageSampler *gdo0_adc_;
  std::string chip_id_;
  bool reset_;
  float output_power_requested_;
  float output_power_effective_;
  uint8_t pa_table_[8];
  union {
    struct CC1101State state_;
    uint8_t regs_[sizeof(struct CC1101State) / sizeof(uint8_t)];
  };

  void strobe_(Command cmd);
  void write_(Register reg);
  void write_(Register reg, uint8_t value);
  void write_(Register reg, uint8_t *buffer, size_t length);
  bool read_(Register reg);
  bool read_(Register reg, uint8_t *buffer, size_t length);
  // bool send_data_(const uint8_t* data, size_t length);
  void send_(Command cmd);
  bool wait_(Command cmd);

  template<class S, class T> void publish_(S *s, T state);
  // template specialization here in the header is not supported by the compiler
  void publish_switch_(switch_::Switch *s, bool state);
  void publish_select_(select::Select *s, size_t index);
};

template<typename... Ts> class BeginTxAction : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(Ts... x) override { this->parent_->begin_tx(); }
};

template<typename... Ts> class EndTxAction : public Action<Ts...>, public Parented<CC1101Component> {
 public:
  void play(Ts... x) override { this->parent_->end_tx(); }
};

template<typename... Ts>
class CC1101RawAction : public remote_base::RCSwitchRawAction<Ts...>, public Parented<CC1101Component> {
 protected:
  void play(Ts... x) override {
    this->parent_->begin_tx();
    remote_base::RCSwitchRawAction<Ts...>::play(x...);
    this->parent_->end_tx();
  }

 public:
};

}  // namespace cc1101
}  // namespace esphome

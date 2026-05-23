#include "ratgdo_number.h"
#include "../ratgdo_state.h"
#include "esphome/core/log.h"

namespace esphome::ratgdo {

static const char* const TAG = "ratgdo.number";

void RATGDONumber::dump_config()
{
    LOG_NUMBER("", "RATGDO Number", this);
    switch (this->number_type_) {
    case RATGDO_OPENING_DURATION:
        ESP_LOGCONFIG(TAG, "  Type: Opening Duration");
        break;
    case RATGDO_CLOSING_DURATION:
        ESP_LOGCONFIG(TAG, "  Type: Closing Duration");
        break;
    default:
        break;
    }
}

void RATGDONumber::setup()
{
    float value;
    this->pref_ = this->make_entity_preference<float>();
    if (!this->pref_.load(&value)) {
        value = 0;
    }
    this->control(value);

    switch (this->number_type_) {
    case RATGDO_OPENING_DURATION:
        this->parent_->subscribe_opening_duration([this](float value) {
            this->update_state(value);
        });
        break;
    case RATGDO_CLOSING_DURATION:
        this->parent_->subscribe_closing_duration([this](float value) {
            this->update_state(value);
        });
        break;
    default:
        break;
    }
}

void RATGDONumber::set_number_type(NumberType number_type_)
{
    this->number_type_ = number_type_;
    switch (this->number_type_) {
    case RATGDO_OPENING_DURATION:
    case RATGDO_CLOSING_DURATION:
        this->traits.set_step(0.1);
        this->traits.set_min_value(0.0);
        this->traits.set_max_value(180.0);
        break;
    default:
        break;
    }
}

void RATGDONumber::update_state(float value)
{
    if (value == this->state) {
        return;
    }
    this->pref_.save(&value);
    this->publish_state(value);
}

void RATGDONumber::control(float value)
{
    switch (this->number_type_) {
    case RATGDO_OPENING_DURATION:
        this->parent_->set_opening_duration(value);
        break;
    case RATGDO_CLOSING_DURATION:
        this->parent_->set_closing_duration(value);
        break;
    default:
        break;
    }
    this->update_state(value);
}

} // namespace esphome::ratgdo

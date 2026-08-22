#pragma once
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../comelit_intercom.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace comelit_intercom {

class ComelitIntercomTextSensor : public text_sensor::TextSensor, public ComelitIntercomFilteredListener {
  public:
    void set_addresses(std::vector<uint8_t> addresses) { this->addresses_ = std::move(addresses); }

    bool matches(uint16_t command, uint16_t address) override {
      return accepts(this->commands_, command) && accepts(this->addresses_, address);
    }
    void on_command(uint16_t command, uint16_t address) override;
    void turn_off(uint32_t *timer) override;

  protected:
    std::vector<uint8_t> addresses_;
};

}  // namespace comelit_intercom
}  // namespace esphome

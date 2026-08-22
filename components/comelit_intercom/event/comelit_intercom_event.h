#pragma once
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../comelit_intercom.h"
#include "esphome/components/event/event.h"

namespace esphome {
namespace comelit_intercom {

class ComelitIntercomEvent : public event::Event, public ComelitIntercomFilteredListener {
  public:
    /// The command list is checked first so that a templated address is only evaluated
    /// for a command this entity can actually report.
    bool matches(uint16_t command, uint16_t address) override {
      return accepts(this->commands_, command) && this->address_.value() == address;
    }
    void on_command(uint16_t command, uint16_t address) override;
};

}  // namespace comelit_intercom
}  // namespace esphome

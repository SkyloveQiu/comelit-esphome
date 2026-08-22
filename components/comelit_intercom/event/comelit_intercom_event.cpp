#include "comelit_intercom_event.h"

namespace esphome {
namespace comelit_intercom {

static const char *const TAG = "comelit.event";

void ComelitIntercomEvent::on_command(uint16_t command, uint16_t address) {
  // Keep in sync with the event_types list built in event/__init__.py.
  this->trigger(std::to_string(command));
}

}  // namespace comelit_intercom
}  // namespace esphome

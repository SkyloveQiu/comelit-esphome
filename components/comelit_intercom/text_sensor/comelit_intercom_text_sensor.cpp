#include "comelit_intercom_text_sensor.h"

namespace esphome {
namespace comelit_intercom {

static const char *const TAG = "comelit.text_sensor";

void ComelitIntercomTextSensor::on_command(uint16_t command, uint16_t address) {
  this->publish_state(str_sprintf("C%u_A%u", command, address));
  if (this->auto_off_ > 0) this->timer_ = millis() + (this->auto_off_ * 1000);
}

void ComelitIntercomTextSensor::turn_off(uint32_t *timer) {
  this->publish_state("");
  *timer = 0;
}

}  // namespace comelit_intercom
}  // namespace esphome

#pragma once
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "../comelit_intercom.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace comelit_intercom {

class ComelitIntercomBinarySensor : public binary_sensor::BinarySensor, public ComelitIntercomListener  {
  public:
    void turn_on(uint32_t *timer, uint16_t auto_off) override;
    void turn_off(uint32_t *timer) override;
};

}  // namespace comelit_intercom
}  // namespace esphome

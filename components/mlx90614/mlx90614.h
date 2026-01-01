#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace mlx90614 {

class MLX90614Component : public PollingComponent, public i2c::I2CDevice {
 public:
  // Sensors
  sensor::Sensor *ambient_sensor{nullptr};
  sensor::Sensor *object_sensor{nullptr};

  // Options
  bool diagnostic_mode{false};

  // Core methods
  void setup() override;
  void dump_config() override;
  void update() override;

  // Setters
  void set_ambient_sensor(sensor::Sensor *s) { this->ambient_sensor = s; }
  void set_object_sensor(sensor::Sensor *s) { this->object_sensor = s; }
  void set_diagnostic_mode(bool v) { this->diagnostic_mode = v; }

 protected:
  bool read_temperature_register_(uint8_t reg, float &out_celsius);
};

}  // namespace mlx90614
}  // namespace esphome
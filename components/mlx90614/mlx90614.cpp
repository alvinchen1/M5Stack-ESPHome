#include "mlx90614.h"
#include "esphome/core/log.h"

namespace esphome {
namespace mlx90614 {

static const char *const TAG = "mlx90614";

// MLX90614 RAM registers
static const uint8_t MLX90614_REG_TA   = 0x06;  // Ambient
static const uint8_t MLX90614_REG_TOBJ = 0x07;  // Object

void MLX90614Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90614...");
}

void MLX90614Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90614 Sensor:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);

  if (this->ambient_sensor != nullptr)
    LOG_SENSOR("  ", "Ambient Temperature", this->ambient_sensor);
  if (this->object_sensor != nullptr)
    LOG_SENSOR("  ", "Object Temperature", this->object_sensor);

  ESP_LOGCONFIG(TAG, "  Diagnostic mode: %s", this->diagnostic_mode ? "ON" : "OFF");
}

void MLX90614Component::update() {
  float ambient_c = NAN;
  float object_c  = NAN;

  bool ok_ambient = this->read_temperature_register_(MLX90614_REG_TA, ambient_c);
  bool ok_object  = this->read_temperature_register_(MLX90614_REG_TOBJ, object_c);

  if (!ok_ambient || !ok_object) {
    ESP_LOGW(TAG, "MLX90614 read failed (ambient_ok=%d, object_ok=%d)", ok_ambient, ok_object);
    this->status_set_warning();
    return;
  }

  this->status_clear_warning();

  if (this->ambient_sensor != nullptr && !isnan(ambient_c))
    this->ambient_sensor->publish_state(ambient_c);

  if (this->object_sensor != nullptr && !isnan(object_c))
    this->object_sensor->publish_state(object_c);
}

bool MLX90614Component::read_temperature_register_(uint8_t reg, float &out_celsius) {
  uint16_t raw = 0;
  auto err = this->read16(reg, &raw);

  if (err != i2c::ERROR_OK) {
    ESP_LOGW(TAG, "I2C read error from reg 0x%02X: %d", reg, err);
    return false;
  }

  // Convert raw value: 0.02 K per LSB
  float temp_k = raw * 0.02f;
  out_celsius = temp_k - 273.15f;

  if (this->diagnostic_mode) {
    ESP_LOGD(TAG, "Reg 0x%02X raw=0x%04X -> %.2f°C", reg, raw, out_celsius);
  }

  return true;
}

}  // namespace mlx90614
}  // namespace esphome
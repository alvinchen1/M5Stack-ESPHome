#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "esphome/core/hal.h"


#ifdef USE_WEBSERVER
#include "esphome/components/web_server_base/web_server_base.h"
#endif

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

namespace esphome {
namespace mlx90640_app {

// Keep as PollingComponent only (camera functionality via web server)
class MLX90640 : public PollingComponent, public i2c::I2CDevice {
public:
  MLX90640() = default;

  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Sensor setters
  void set_min_temperature_sensor(sensor::Sensor *min_temperature_sensor) {
    this->min_temperature_sensor_ = min_temperature_sensor;
  }
  void set_max_temperature_sensor(sensor::Sensor *max_temperature_sensor) {
    this->max_temperature_sensor_ = max_temperature_sensor;
  }
  void set_mean_temperature_sensor(sensor::Sensor *mean_temperature_sensor) {
    this->mean_temperature_sensor_ = mean_temperature_sensor;
  }
  void
  set_median_temperature_sensor(sensor::Sensor *median_temperature_sensor) {
    this->median_temperature_sensor_ = median_temperature_sensor;
  }

  // Configuration setters
  // SDA/SCL/Frequency/Address handled by I2CDevice
  void set_mintemp(float mintemp) { this->mintemp_ = mintemp; }
  void set_maxtemp(float maxtemp) { this->maxtemp_ = maxtemp; }
  void set_refresh_rate(uint8_t refresh_rate) {
    this->refresh_rate_ = refresh_rate;
  }

#ifdef USE_WEBSERVER
  void set_base(web_server_base::WebServerBase *base) { this->base_ = base; }

  // Method to get current image for web server
  std::vector<uint8_t> get_current_image() { return this->current_image_; }
  bool is_image_ready() { return this->image_ready_; }
#endif

protected:
  // Sensor outputs
  sensor::Sensor *min_temperature_sensor_{nullptr};
  sensor::Sensor *max_temperature_sensor_{nullptr};
  sensor::Sensor *mean_temperature_sensor_{nullptr};
  sensor::Sensor *median_temperature_sensor_{nullptr};

  // Configuration
  // sda_pin_, scl_pin_, frequency_, address_ inherited/managed by I2CDevice
  float mintemp_;
  float maxtemp_;
  uint8_t refresh_rate_;

#ifdef USE_WEBSERVER
  web_server_base::WebServerBase *base_{nullptr};
#endif

  // MLX90640 specific
  paramsMLX90640 mlx90640_;
  uint16_t eeMLX90640_[832];
  float mlx90640To_[768]; // 32x24 thermal array
  float min_value_;
  float max_value_;

  // Image data for web server
  std::vector<uint8_t> current_image_;
  bool image_ready_{false};
  uint32_t last_frame_time_{0};

  // Helper methods
  void read_thermal_data_();
  void generate_bmp_image_();
  void publish_sensors_();
  void apply_color_map_(float normalized_value, uint8_t &r, uint8_t &g,
                        uint8_t &b);
};

} // namespace mlx90640_app
} // namespace esphome

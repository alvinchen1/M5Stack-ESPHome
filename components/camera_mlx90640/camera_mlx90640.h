#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#ifdef USE_WEBSERVER
#include "esphome/components/web_server_base/web_server_base.h"
#endif

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

namespace esphome {
namespace mlx90640_app {

class MLX90640 : public PollingComponent, public i2c::I2CDevice {
 public:
  MLX90640() = default;

  // Core ESPHome lifecycle
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Sensor setters
  void set_min_temperature_sensor(sensor::Sensor *s) { this->min_temperature_sensor_ = s; }
  void set_max_temperature_sensor(sensor::Sensor *s) { this->max_temperature_sensor_ = s; }
  void set_mean_temperature_sensor(sensor::Sensor *s) { this->mean_temperature_sensor_ = s; }
  void set_median_temperature_sensor(sensor::Sensor *s) { this->median_temperature_sensor_ = s; }

  // Configuration setters
  void set_sda_pin(int sda) { this->sda_pin_ = sda; }
  void set_scl_pin(int scl) { this->scl_pin_ = scl; }
  void set_frequency(int freq) { this->frequency_ = freq; }
  void set_address(uint8_t addr) { this->address_ = addr; }
  void set_mintemp(float t) { this->mintemp_ = t; }
  void set_maxtemp(float t) { this->maxtemp_ = t; }
  void set_refresh_rate(uint8_t rate) { this->refresh_rate_ = rate; }

#ifdef USE_WEBSERVER
  void set_base(web_server_base::WebServerBase *b) { this->base_ = b; }

  // Web server accessors
  std::vector<uint8_t> get_current_image() { return this->current_image_; }
  bool is_image_ready() const { return this->image_ready_; }
#endif

 protected:
  // Sensor outputs
  sensor::Sensor *min_temperature_sensor_{nullptr};
  sensor::Sensor *max_temperature_sensor_{nullptr};
  sensor::Sensor *mean_temperature_sensor_{nullptr};
  sensor::Sensor *median_temperature_sensor_{nullptr};

  // Configuration
  int sda_pin_{-1};
  int scl_pin_{-1};
  int frequency_{400000};
  uint8_t address_{0x33};
  float mintemp_{15.0f};
  float maxtemp_{40.0f};
  uint8_t refresh_rate_{0x04};

#ifdef USE_WEBSERVER
  web_server_base::WebServerBase *base_{nullptr};
#endif

  // MLX90640 internal structures
  paramsMLX90640 mlx90640_;
  uint16_t eeMLX90640_[832];
  float mlx90640To_[768];  // 32x24 thermal array

  float min_value_{0};
  float max_value_{0};

  // Image buffer for web server
  std::vector<uint8_t> current_image_;
  bool image_ready_{false};
  uint32_t last_frame_time_{0};

  // Internal helpers
  void read_thermal_data_();
  void generate_bmp_image_();
  void publish_sensors_();
  void apply_color_map_(float normalized_value, uint8_t &r, uint8_t &g, uint8_t &b);
};

}  // namespace mlx90640_app
}  // namespace esphome
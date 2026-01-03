#pragma once
// Define this to enable camera support
// #define USE_MLX90640_CAMERA

#ifdef USE_MLX90640_CAMERA
#include "esphome/components/camera/camera.h"
#endif

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include <mutex>

#ifdef USE_MLX90640_WEB_SERVER
#include "esphome/components/web_server/web_server.h"
#endif

#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"

#include <vector>

namespace esphome {
namespace mlx90640 {

static const uint16_t REQUEST_IMAGE_WIDTH = 32;
static const uint16_t REQUEST_IMAGE_HEIGHT = 24;

class MLX90640Component : public PollingComponent, public i2c::I2CDevice {
public:
#ifdef USE_MLX90640_WEB_SERVER
  void start_stream_server();
#endif

  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  void set_min_temperature_sensor(sensor::Sensor *s) {
    min_temperature_sensor_ = s;
  }
  void set_max_temperature_sensor(sensor::Sensor *s) {
    max_temperature_sensor_ = s;
  }
  void set_mean_temperature_sensor(sensor::Sensor *s) {
    mean_temperature_sensor_ = s;
  }
  void set_median_temperature_sensor(sensor::Sensor *s) {
    median_temperature_sensor_ = s;
  }

  void set_emissivity(float emissivity) { emissivity_ = emissivity; }
  void set_refresh_rate(int refresh_rate) { refresh_rate_ = refresh_rate; }
  void set_min_image_temp(float t) { min_image_temp_ = t; }
  void set_max_image_temp(float t) { max_image_temp_ = t; }

  // Method to get the latest image data
  void get_image_data(std::vector<uint8_t> &data);

  // Helper to get raw data for camera if needed
  float *get_thermal_data() { return mlx90640_to_; }

protected:
#ifdef USE_MLX90640_WEB_SERVER
  bool stream_server_started_{false};
#endif
  std::mutex image_lock_;
  sensor::Sensor *min_temperature_sensor_{nullptr};
  sensor::Sensor *max_temperature_sensor_{nullptr};
  sensor::Sensor *mean_temperature_sensor_{nullptr};
  sensor::Sensor *median_temperature_sensor_{nullptr};

  float emissivity_{0.95};
  int refresh_rate_{2}; // Default 2Hz
  float min_image_temp_{0.0f};
  float max_image_temp_{300.0f};

  // MLX90640 Driver Data
  paramsMLX90640 mlx90640_params_;
  // ee_mlx90640 not used typically? Driver uses its own buffer or we pass one?
  // MLX90640_DumpEE writes to array.

  float mlx90640_to_[768];
  uint16_t mlx90640_frame_[834];

  // Image buffer (RGB565)
  std::vector<uint8_t> image_buffer_;

  void set_refresh_rate_hw_();
};

// Define this to enable web server image handler
// Moved to top

#ifdef USE_MLX90640_CAMERA
class MLX90640Camera : public camera::Camera,
                       public Parented<MLX90640Component> {
public:
  void setup() override;
  void loop() override;
  void request_image(camera::CameraRequester requester) override;
};
#endif

#ifdef USE_MLX90640_WEB_SERVER
#include <esp_http_server.h>
// Handler for serving BMP image
esp_err_t mlx90640_web_server_handler(httpd_req_t *req);
#endif

} // namespace mlx90640
} // namespace esphome

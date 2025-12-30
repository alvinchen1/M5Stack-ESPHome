#include "camera_mlx90640.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <FS.h>
#include <SPIFFS.h>

namespace esphome {
namespace mlx90640_app {

static const char *const TAG = "MLX90640";

#ifdef USE_WEBSERVER
// Custom web handler for thermal images
class ThermalImageHandler : public AsyncWebHandler {
public:
  ThermalImageHandler(MLX90640 *parent) : parent_(parent) {}

  bool canHandle(AsyncWebServerRequest *request) {
    if (request->method() != HTTP_GET) {
      return false;
    }
    std::string url = request->url();
    return url == "/thermal-camera" || url == "/thermal.bmp";
  }

  void handleRequest(AsyncWebServerRequest *request) {
    if (!this->parent_->is_image_ready()) {
      request->send(404, "text/plain", "No thermal image available");
      return;
    }

    std::vector<uint8_t> image = this->parent_->get_current_image();
    if (image.empty()) {
      request->send(500, "text/plain", "Failed to generate thermal image");
      return;
    }

    // Send BMP image as binary data
    AsyncWebServerResponse *response =
        request->beginResponse(200, "image/bmp", image.data(), image.size());

    response->addHeader("Content-Disposition", "inline; filename=thermal.bmp");
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    request->send(response);
  }

protected:
  MLX90640 *parent_;
};
#endif

// ============================================================================
// Setup
// ============================================================================

void MLX90640::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90640...");

  // Initialize I2C
  // Managed by ESPHome I2C component
  MLX90640_SetDevice(this);

  // Check if sensor is connected
  if (!MLX90640_I2CRead(0, 0x2400, 1, this->eeMLX90640_)) {
    ESP_LOGE(TAG, "Failed to read from MLX90640 - is it connected?");
    this->mark_failed();
    return;
  }

  // Extract parameters
  int status = MLX90640_ExtractParameters(this->eeMLX90640_, &this->mlx90640_);
  if (status != 0) {
    ESP_LOGE(TAG, "Failed to extract MLX90640 parameters");
    this->mark_failed();
    return;
  }

  // Set refresh rate
  MLX90640_SetRefreshRate(0, this->refresh_rate_);

  // Initialize SPIFFS (optional, for backward compatibility)
  if (!SPIFFS.begin(true)) {
    ESP_LOGD(TAG, "SPIFFS not available");
  }

  // Setup web server handler
#ifdef USE_WEBSERVER
  if (this->base_ != nullptr) {
    ESP_LOGI(TAG, "Registering thermal camera web handler");
    this->base_->add_handler(new ThermalImageHandler(this));
  }
#endif

  ESP_LOGCONFIG(TAG, "MLX90640 setup complete");
}

// ============================================================================
// Update - Called periodically
// ============================================================================

void MLX90640::update() {
  ESP_LOGV(TAG, "Updating MLX90640 data...");

  // Read thermal data from sensor
  this->read_thermal_data_();

  // Generate BMP image
  this->generate_bmp_image_();

  // Publish temperature sensors to Home Assistant
  this->publish_sensors_();

  // Mark image as ready
  this->image_ready_ = true;
  this->last_frame_time_ = millis();
}

// ============================================================================
// Loop - Called continuously
// ============================================================================

void MLX90640::loop() {
  // Nothing needed in loop
}

// ============================================================================
// Dump Config
// ============================================================================

void MLX90640::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90640:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  Min Temp: %.1f°C", this->mintemp_);
  ESP_LOGCONFIG(TAG, "  Max Temp: %.1f°C", this->maxtemp_);
  ESP_LOGCONFIG(TAG, "  Refresh Rate: 0x%02X", this->refresh_rate_);

  LOG_SENSOR("  ", "Min Temperature", this->min_temperature_sensor_);
  LOG_SENSOR("  ", "Max Temperature", this->max_temperature_sensor_);
  LOG_SENSOR("  ", "Mean Temperature", this->mean_temperature_sensor_);
  LOG_SENSOR("  ", "Median Temperature", this->median_temperature_sensor_);

#ifdef USE_WEBSERVER
  if (this->base_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Web Server: Available at /thermal-camera");
  }
#endif
}

// ============================================================================
// Read Thermal Data from MLX90640
// ============================================================================

void MLX90640::read_thermal_data_() {
  uint16_t frame[834];
  int status;

  // Get frame data
  for (int retry = 0; retry < 3; retry++) {
    status = MLX90640_GetFrameData(0, frame);
    if (status >= 0) {
      break;
    }
    ESP_LOGW(TAG, "Failed to get frame data, retry %d/3", retry + 1);
    delay(10);
  }

  if (status < 0) {
    ESP_LOGE(TAG, "Failed to read frame data from MLX90640");
    return;
  }

  // Calculate temperatures
  float vdd = MLX90640_GetVdd(frame, &this->mlx90640_);
  float ta = MLX90640_GetTa(frame, &this->mlx90640_);
  float tr = ta - 8.0f;
  float emissivity = 0.95f;

  MLX90640_CalculateTo(frame, &this->mlx90640_, emissivity, tr,
                       this->mlx90640To_);

  ESP_LOGV(TAG, "Frame read complete - Ta: %.1f°C, Vdd: %.2fV", ta, vdd);
}

// ============================================================================
// Generate BMP Image
// ============================================================================

void MLX90640::generate_bmp_image_() {
  const int width = 32;
  const int height = 24;
  const int scale = 10; // 320x240 output
  const int scaled_width = width * scale;
  const int scaled_height = height * scale;

  // Calculate min/max temperatures
  this->min_value_ = this->mlx90640To_[0];
  this->max_value_ = this->mlx90640To_[0];

  for (int i = 0; i < 768; i++) {
    float temp = this->mlx90640To_[i];
    if (temp < this->min_value_)
      this->min_value_ = temp;
    if (temp > this->max_value_)
      this->max_value_ = temp;
  }

  // Clamp to configured range
  if (this->min_value_ < this->mintemp_)
    this->min_value_ = this->mintemp_;
  if (this->max_value_ > this->maxtemp_)
    this->max_value_ = this->maxtemp_;

  float temp_range = this->max_value_ - this->min_value_;
  if (temp_range < 1.0f)
    temp_range = 1.0f;

  // BMP header (54 bytes) + pixel data (320x240x3)
  const int row_size =
      ((scaled_width * 3 + 3) / 4) * 4; // Row must be multiple of 4
  const int pixel_data_size = row_size * scaled_height;
  const int file_size = 54 + pixel_data_size;

  this->current_image_.resize(file_size);
  uint8_t *data = this->current_image_.data();

  // BMP File Header (14 bytes)
  data[0] = 'B';
  data[1] = 'M';                   // Signature
  memcpy(&data[2], &file_size, 4); // File size
  data[6] = 0;
  data[7] = 0; // Reserved
  data[8] = 0;
  data[9] = 0; // Reserved
  uint32_t pixel_offset = 54;
  memcpy(&data[10], &pixel_offset, 4); // Pixel data offset

  // BMP Info Header (40 bytes)
  uint32_t header_size = 40;
  memcpy(&data[14], &header_size, 4);
  memcpy(&data[18], &scaled_width, 4);
  memcpy(&data[22], &scaled_height, 4);
  uint16_t planes = 1;
  memcpy(&data[26], &planes, 2);
  uint16_t bits_per_pixel = 24;
  memcpy(&data[28], &bits_per_pixel, 2);
  uint32_t compression = 0;
  memcpy(&data[30], &compression, 4);
  memcpy(&data[34], &pixel_data_size, 4);
  uint32_t ppm = 2835;        // 72 DPI
  memcpy(&data[38], &ppm, 4); // X pixels per meter
  memcpy(&data[42], &ppm, 4); // Y pixels per meter
  uint32_t colors = 0;
  memcpy(&data[46], &colors, 4);
  memcpy(&data[50], &colors, 4);

  // Generate pixel data (BMP is bottom-up)
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int idx = y * width + x;
      float temp = this->mlx90640To_[idx];
      float normalized = (temp - this->min_value_) / temp_range;
      if (normalized < 0.0f)
        normalized = 0.0f;
      if (normalized > 1.0f)
        normalized = 1.0f;

      uint8_t r, g, b;
      this->apply_color_map_(normalized, r, g, b);

      // Scale and write pixels (bottom-up)
      for (int sy = 0; sy < scale; sy++) {
        for (int sx = 0; sx < scale; sx++) {
          int bmp_y = (scaled_height - 1) - (y * scale + sy); // Flip vertically
          int bmp_x = x * scale + sx;
          int pixel_offset = 54 + (bmp_y * row_size) + (bmp_x * 3);

          data[pixel_offset + 0] = b; // BMP is BGR
          data[pixel_offset + 1] = g;
          data[pixel_offset + 2] = r;
        }
      }
    }
  }

  ESP_LOGV(TAG, "Generated BMP image: %dx%d, %.1f-%.1f°C", scaled_width,
           scaled_height, this->min_value_, this->max_value_);
}

// ============================================================================
// Apply Thermal Color Map
// ============================================================================

void MLX90640::apply_color_map_(float value, uint8_t &r, uint8_t &g,
                                uint8_t &b) {
  if (value < 0.0f)
    value = 0.0f;
  if (value > 1.0f)
    value = 1.0f;

  // Ironbow colormap
  if (value < 0.2f) {
    float t = value * 5.0f;
    r = 0;
    g = 0;
    b = static_cast<uint8_t>(100 + t * 155);
  } else if (value < 0.4f) {
    float t = (value - 0.2f) * 5.0f;
    r = 0;
    g = static_cast<uint8_t>(t * 255);
    b = 255;
  } else if (value < 0.6f) {
    float t = (value - 0.4f) * 5.0f;
    r = 0;
    g = 255;
    b = static_cast<uint8_t>((1.0f - t) * 255);
  } else if (value < 0.8f) {
    float t = (value - 0.6f) * 5.0f;
    r = static_cast<uint8_t>(t * 255);
    g = 255;
    b = 0;
  } else {
    float t = (value - 0.8f) * 5.0f;
    r = 255;
    g = static_cast<uint8_t>(255 - t * 100);
    b = static_cast<uint8_t>(t * 200);
  }
}

// ============================================================================
// Publish Temperature Sensors
// ============================================================================

void MLX90640::publish_sensors_() {
  if (this->min_temperature_sensor_ != nullptr) {
    this->min_temperature_sensor_->publish_state(this->min_value_);
  }

  if (this->max_temperature_sensor_ != nullptr) {
    this->max_temperature_sensor_->publish_state(this->max_value_);
  }

  if (this->mean_temperature_sensor_ != nullptr) {
    float sum = 0.0f;
    for (int i = 0; i < 768; i++) {
      sum += this->mlx90640To_[i];
    }
    this->mean_temperature_sensor_->publish_state(sum / 768.0f);
  }

  if (this->median_temperature_sensor_ != nullptr) {
    std::vector<float> temps(this->mlx90640To_, this->mlx90640To_ + 768);
    std::sort(temps.begin(), temps.end());
    this->median_temperature_sensor_->publish_state(temps[384]);
  }

  ESP_LOGV(TAG, "Published sensors - Min: %.1f°C, Max: %.1f°C",
           this->min_value_, this->max_value_);
}

} // namespace mlx90640_app
} // namespace esphome

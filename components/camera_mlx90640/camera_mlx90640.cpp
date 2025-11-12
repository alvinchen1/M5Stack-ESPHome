#include "camera_mlx90640.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <FS.h>
#include <SPIFFS.h>

namespace esphome {
namespace mlx90640_app {

static const char *const TAG = "MLX90640";

// ============================================================================
// Setup
// ============================================================================

void MLX90640::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90640...");

  // Initialize I2C
  Wire.begin(this->sda_pin_, this->scl_pin_);
  Wire.setClock(this->frequency_);

  // Check if sensor is connected
  if (!MLX90640_I2CRead(this->address_, 0x2400, 1, this->eeMLX90640_)) {
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
  MLX90640_SetRefreshRate(this->address_, this->refresh_rate_);

  // Initialize SPIFFS (for backward compatibility with web server if enabled)
  if (!SPIFFS.begin(true)) {
    ESP_LOGW(TAG, "Failed to mount SPIFFS");
  }

  // Web server handler (keep for backward compatibility)
#ifdef USE_WEBSERVER
  if (this->base_ != nullptr) {
    ESP_LOGI(TAG, "Web server support available but not used for camera streaming");
    // Removed old web server code - camera now streams via ESPHome API
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

  // Generate camera image for streaming
  this->generate_camera_image_();

  // Publish temperature sensors to Home Assistant
  this->publish_sensors_();

  // Mark image as ready for camera requests
  this->image_ready_ = true;
  this->last_frame_time_ = millis();
}

// ============================================================================
// Loop - Called continuously
// ============================================================================

void MLX90640::loop() {
  // Nothing needed in loop for now
  // All updates are handled in update() based on update_interval
}

// ============================================================================
// Dump Config
// ============================================================================

void MLX90640::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90640:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  SDA Pin: %d", this->sda_pin_);
  ESP_LOGCONFIG(TAG, "  SCL Pin: %d", this->scl_pin_);
  ESP_LOGCONFIG(TAG, "  Frequency: %d Hz", this->frequency_);
  ESP_LOGCONFIG(TAG, "  Address: 0x%02X", this->address_);
  ESP_LOGCONFIG(TAG, "  Min Temp: %.1f°C", this->mintemp_);
  ESP_LOGCONFIG(TAG, "  Max Temp: %.1f°C", this->maxtemp_);
  ESP_LOGCONFIG(TAG, "  Refresh Rate: 0x%02X", this->refresh_rate_);
  ESP_LOGCONFIG(TAG, "  Camera Streaming: ENABLED");

  LOG_SENSOR("  ", "Min Temperature", this->min_temperature_sensor_);
  LOG_SENSOR("  ", "Max Temperature", this->max_temperature_sensor_);
  LOG_SENSOR("  ", "Mean Temperature", this->mean_temperature_sensor_);
  LOG_SENSOR("  ", "Median Temperature", this->median_temperature_sensor_);
}

// ============================================================================
// Camera Interface - Request Image
// ============================================================================

void MLX90640::request_image(camera::Camera::RequestType type) {
  ESP_LOGD(TAG, "Camera image requested (type: %d)", static_cast<int>(type));
  
  // If we have a recent image, return it immediately
  if (this->image_ready_ && (millis() - this->last_frame_time_ < 5000)) {
    ESP_LOGV(TAG, "Using cached thermal image");
    return;
  }

  // Otherwise trigger a new update
  ESP_LOGD(TAG, "Capturing new thermal frame");
  this->image_ready_ = false;
  this->update();
}

// ============================================================================
// Camera Interface - Get Snapshot
// ============================================================================

camera::CameraImageData *MLX90640::get_snapshot() {
  if (!this->image_ready_ || this->current_image_.empty()) {
    ESP_LOGW(TAG, "No thermal image available for snapshot");
    return nullptr;
  }

  ESP_LOGD(TAG, "Returning thermal snapshot (%d bytes)", this->current_image_.size());

  // Create and return camera image data
  // Note: CameraImageData will handle memory management
  auto *image = new camera::CameraImageData(
      this->current_image_.data(),
      this->current_image_.size(),
      camera::ImageType::IMAGE_TYPE_RGB24
  );

  return image;
}

// ============================================================================
// Read Thermal Data from MLX90640
// ============================================================================

void MLX90640::read_thermal_data_() {
  uint16_t frame[834];
  int status;

  // Get frame data
  for (int retry = 0; retry < 3; retry++) {
    status = MLX90640_GetFrameData(this->address_, frame);
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
  float tr = ta - 8.0f;  // Reflected temperature (Ta - 8°C is typical)
  float emissivity = 0.95f;

  MLX90640_CalculateTo(frame, &this->mlx90640_, emissivity, tr, this->mlx90640To_);

  ESP_LOGV(TAG, "Frame read complete - Ta: %.1f°C, Vdd: %.2fV", ta, vdd);
}

// ============================================================================
// Generate Camera Image (RGB24 format)
// ============================================================================

void MLX90640::generate_camera_image_() {
  const int width = 32;   // MLX90640 native resolution
  const int height = 24;
  const int scale = 10;   // Upscale factor (320x240 final image)
  const int scaled_width = width * scale;
  const int scaled_height = height * scale;

  // Calculate min/max temperatures for normalization
  this->min_value_ = this->mlx90640To_[0];
  this->max_value_ = this->mlx90640To_[0];

  for (int i = 0; i < 768; i++) {
    float temp = this->mlx90640To_[i];
    if (temp < this->min_value_) this->min_value_ = temp;
    if (temp > this->max_value_) this->max_value_ = temp;
  }

  // Clamp to configured range if specified
  if (this->min_value_ < this->mintemp_) this->min_value_ = this->mintemp_;
  if (this->max_value_ > this->maxtemp_) this->max_value_ = this->maxtemp_;

  float temp_range = this->max_value_ - this->min_value_;
  if (temp_range < 1.0f) temp_range = 1.0f;  // Avoid division by zero

  // Allocate RGB24 image buffer (3 bytes per pixel)
  size_t image_size = scaled_width * scaled_height * 3;
  this->current_image_.resize(image_size);

  ESP_LOGV(TAG, "Generating %dx%d thermal image (temp range: %.1f-%.1f°C)",
           scaled_width, scaled_height, this->min_value_, this->max_value_);

  // Generate scaled thermal image with color mapping
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      // Get temperature value
      int idx = y * width + x;
      float temp = this->mlx90640To_[idx];

      // Normalize to 0-1 range
      float normalized = (temp - this->min_value_) / temp_range;
      if (normalized < 0.0f) normalized = 0.0f;
      if (normalized > 1.0f) normalized = 1.0f;

      // Apply thermal color map
      uint8_t r, g, b;
      this->apply_color_map_(normalized, r, g, b);

      // Scale up the pixel (10x10 block for each thermal pixel)
      for (int sy = 0; sy < scale; sy++) {
        for (int sx = 0; sx < scale; sx++) {
          int scaled_x = x * scale + sx;
          int scaled_y = y * scale + sy;
          int pixel_idx = (scaled_y * scaled_width + scaled_x) * 3;

          this->current_image_[pixel_idx + 0] = r;
          this->current_image_[pixel_idx + 1] = g;
          this->current_image_[pixel_idx + 2] = b;
        }
      }
    }
  }

  ESP_LOGV(TAG, "Thermal image generation complete");
}

// ============================================================================
// Apply Thermal Color Map (Ironbow style)
// ============================================================================

void MLX90640::apply_color_map_(float value, uint8_t &r, uint8_t &g, uint8_t &b) {
  // Clamp value to 0-1
  if (value < 0.0f) value = 0.0f;
  if (value > 1.0f) value = 1.0f;

  // Ironbow thermal colormap
  // Cold (0.0) = Dark Blue
  // Medium (0.5) = Green/Yellow  
  // Hot (1.0) = Red/White

  if (value < 0.2f) {
    // Dark blue to blue
    float t = value * 5.0f;
    r = 0;
    g = 0;
    b = static_cast<uint8_t>(100 + t * 155);
  } else if (value < 0.4f) {
    // Blue to cyan
    float t = (value - 0.2f) * 5.0f;
    r = 0;
    g = static_cast<uint8_t>(t * 255);
    b = 255;
  } else if (value < 0.6f) {
    // Cyan to green
    float t = (value - 0.4f) * 5.0f;
    r = 0;
    g = 255;
    b = static_cast<uint8_t>((1.0f - t) * 255);
  } else if (value < 0.8f) {
    // Green to yellow to orange
    float t = (value - 0.6f) * 5.0f;
    r = static_cast<uint8_t>(t * 255);
    g = 255;
    b = 0;
  } else {
    // Orange to red to white (hottest)
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
  // Publish min temperature
  if (this->min_temperature_sensor_ != nullptr) {
    this->min_temperature_sensor_->publish_state(this->min_value_);
  }

  // Publish max temperature
  if (this->max_temperature_sensor_ != nullptr) {
    this->max_temperature_sensor_->publish_state(this->max_value_);
  }

  // Calculate and publish mean temperature
  if (this->mean_temperature_sensor_ != nullptr) {
    float sum = 0.0f;
    for (int i = 0; i < 768; i++) {
      sum += this->mlx90640To_[i];
    }
    float mean = sum / 768.0f;
    this->mean_temperature_sensor_->publish_state(mean);
  }

  // Calculate and publish median temperature
  if (this->median_temperature_sensor_ != nullptr) {
    // Copy array for sorting
    std::vector<float> temps(this->mlx90640To_, this->mlx90640To_ + 768);
    std::sort(temps.begin(), temps.end());
    float median = temps[384];  // Middle value of 768 elements
    this->median_temperature_sensor_->publish_state(median);
  }

  ESP_LOGV(TAG, "Published sensor values - Min: %.1f°C, Max: %.1f°C",
           this->min_value_, this->max_value_);
}

}  // namespace mlx90640_app
}  // namespace esphome

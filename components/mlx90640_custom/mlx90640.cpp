#include "mlx90640.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include <algorithm>

namespace esphome {
namespace mlx90640 {

static const char *const TAG = "mlx90640";

// Global buffer for web server response (simple non-concurrent solution)
#ifdef USE_MLX90640_WEB_SERVER
static std::vector<uint8_t> g_bmp_buffer;
#endif

// Palette from Chill-Division/M5Stack-ESPHome
static const uint16_t camColors[] = {
    0x480F, 0x400F, 0x400F, 0x400F, 0x4010, 0x3810, 0x3810, 0x3810, 0x3810,
    0x3010, 0x3010, 0x3010, 0x2810, 0x2810, 0x2810, 0x2810, 0x2010, 0x2010,
    0x2010, 0x1810, 0x1810, 0x1811, 0x1811, 0x1011, 0x1011, 0x1011, 0x0811,
    0x0811, 0x0811, 0x0011, 0x0011, 0x0011, 0x0011, 0x0011, 0x0031, 0x0031,
    0x0051, 0x0072, 0x0072, 0x0092, 0x00B2, 0x00B2, 0x00D2, 0x00F2, 0x00F2,
    0x0112, 0x0132, 0x0152, 0x0152, 0x0172, 0x0192, 0x0192, 0x01B2, 0x01D2,
    0x01F3, 0x01F3, 0x0213, 0x0233, 0x0253, 0x0253, 0x0273, 0x0293, 0x02B3,
    0x02D3, 0x02D3, 0x02F3, 0x0313, 0x0333, 0x0333, 0x0353, 0x0373, 0x0394,
    0x03B4, 0x03D4, 0x03D4, 0x03F4, 0x0414, 0x0434, 0x0454, 0x0474, 0x0474,
    0x0494, 0x04B4, 0x04D4, 0x04F4, 0x0514, 0x0534, 0x0534, 0x0554, 0x0554,
    0x0574, 0x0574, 0x0573, 0x0573, 0x0573, 0x0572, 0x0572, 0x0572, 0x0571,
    0x0591, 0x0591, 0x0590, 0x0590, 0x058F, 0x058F, 0x058F, 0x058E, 0x05AE,
    0x05AE, 0x05AD, 0x05AD, 0x05AD, 0x05AC, 0x05AC, 0x05AB, 0x05CB, 0x05CB,
    0x05CA, 0x05CA, 0x05CA, 0x05C9, 0x05C9, 0x05C8, 0x05E8, 0x05E8, 0x05E7,
    0x05E7, 0x05E6, 0x05E6, 0x05E6, 0x05E5, 0x05E5, 0x0604, 0x0604, 0x0604,
    0x0603, 0x0603, 0x0602, 0x0602, 0x0601, 0x0621, 0x0621, 0x0620, 0x0620,
    0x0620, 0x0620, 0x0E20, 0x0E20, 0x0E40, 0x1640, 0x1640, 0x1E40, 0x1E40,
    0x2640, 0x2640, 0x2E40, 0x2E60, 0x3660, 0x3660, 0x3E60, 0x3E60, 0x3E60,
    0x4660, 0x4660, 0x4E60, 0x4E80, 0x5680, 0x5680, 0x5E80, 0x5E80, 0x6680,
    0x6680, 0x6E80, 0x6EA0, 0x76A0, 0x76A0, 0x7EA0, 0x7EA0, 0x86A0, 0x86A0,
    0x8EA0, 0x8EC0, 0x96C0, 0x96C0, 0x9EC0, 0x9EC0, 0xA6C0, 0xAEC0, 0xAEC0,
    0xB6E0, 0xB6E0, 0xBEE0, 0xBEE0, 0xC6E0, 0xC6E0, 0xCEE0, 0xCEE0, 0xD6E0,
    0xD700, 0xDF00, 0xDEE0, 0xDEC0, 0xDEA0, 0xDE80, 0xDE80, 0xE660, 0xE640,
    0xE620, 0xE600, 0xE5E0, 0xE5C0, 0xE5A0, 0xE580, 0xE560, 0xE540, 0xE520,
    0xE500, 0xE4E0, 0xE4C0, 0xE4A0, 0xE480, 0xE460, 0xEC40, 0xEC20, 0xEC00,
    0xEBE0, 0xEBC0, 0xEBA0, 0xEB80, 0xEB60, 0xEB40, 0xEB20, 0xEB00, 0xEAE0,
    0xEAC0, 0xEAA0, 0xEA80, 0xEA60, 0xEA40, 0xF220, 0xF200, 0xF1E0, 0xF1C0,
    0xF1A0, 0xF180, 0xF160, 0xF140, 0xF100, 0xF0E0, 0xF0C0, 0xF0A0, 0xF080,
    0xF060, 0xF040, 0xF020, 0xF800,
};

void MLX90640Component::setup() {
  ESP_LOGCONFIG(TAG, "Setting up MLX90640...");

  // Init I2C Driver with this device
  MLX90640_SetDevice(this);

  // Check connection
  int status;
  uint16_t ee_data[832];
  status = MLX90640_DumpEE(this->address_, ee_data);

  if (status != 0) {
    ESP_LOGE(TAG, "Failed to dump EEPROM data");
    this->mark_failed();
    return;
  }

  // Calibration parameters
  status = MLX90640_ExtractParameters(ee_data, &this->mlx90640_params_);
  if (status != 0) {
    ESP_LOGE(TAG, "Failed to extract parameters");
    this->mark_failed();
    return;
  }

  // Set refresh rate
  this->set_refresh_rate_hw_();

  ESP_LOGCONFIG(TAG, "MLX90640 Setup Complete");
}

void MLX90640Component::loop() {
  PollingComponent::loop();
#ifdef USE_MLX90640_WEB_SERVER
  if (!this->stream_server_started_) {
    // Wait for 10 seconds to ensure network stack (LwIP) is initialized
    if (millis() > 10000) {
      this->start_stream_server();
      this->stream_server_started_ = true;
    }
  }
#endif
}

#ifdef USE_MLX90640_WEB_SERVER
void MLX90640Component::start_stream_server() {
  static httpd_handle_t thermal_server = NULL;

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 8080;
  config.ctrl_port = 32769; // Different control port to avoid 32768 conflict
  config.stack_size =
      8192; // Increase stack size to prevent crashes on image processing

  // Lower stack size if needed, but default (4096) is usually safe for this
  // simple handler
  config.stack_size = 8192; // Increased stack size for web server handler

  if (httpd_start(&thermal_server, &config) == ESP_OK) {
    httpd_uri_t thermal_uri = {.uri = "/thermal.bmp",
                               .method = HTTP_GET,
                               .handler = mlx90640_web_server_handler,
                               .user_ctx = this};
    httpd_register_uri_handler(thermal_server, &thermal_uri);
    ESP_LOGI(TAG, "Thermal Camera Server started on port 8080");
  } else {
    ESP_LOGE(TAG, "Failed to start Thermal Camera Server");
  }
}
#endif

void MLX90640Component::dump_config() {
  ESP_LOGCONFIG(TAG, "MLX90640:");
  LOG_I2C_DEVICE(this);
  LOG_SENSOR("  ", "Min Temperature", this->min_temperature_sensor_);
  LOG_SENSOR("  ", "Max Temperature", this->max_temperature_sensor_);
  LOG_SENSOR("  ", "Mean Temperature", this->mean_temperature_sensor_);
  LOG_SENSOR("  ", "Median Temperature", this->median_temperature_sensor_);
  ESP_LOGCONFIG(TAG, "  Refresh Rate: %d Hz", this->refresh_rate_);
}

void MLX90640Component::update() {
  int status = MLX90640_GetFrameData(this->address_, this->mlx90640_frame_);
  if (status < 0) {
    ESP_LOGW(TAG, "GetFrameData failed! %d", status);
    return;
  }

  float vdd = MLX90640_GetVdd(this->mlx90640_frame_, &this->mlx90640_params_);
  float ta = MLX90640_GetTa(this->mlx90640_frame_, &this->mlx90640_params_);

  float tr = ta - 8.0f; // Reflected temperature assumed to be Ta - 8

  MLX90640_CalculateTo(this->mlx90640_frame_, &this->mlx90640_params_,
                       this->emissivity_, tr, this->mlx90640_to_);

  // ----------------------------------

  float min_temp = 1000.0f;
  float max_temp = -1000.0f;
  float sum_temp = 0.0f;
  std::vector<float> sorted_temps;
  sorted_temps.reserve(768);

  // Lock for buffer modification
  std::lock_guard<std::mutex> lock(this->image_lock_);

  // Process data & populate image buffer
  // We'll create an RGB565 buffer directly for the camera
  // But wait, the camera component typically streams JPEG or RGB565.
  // Let's stick to RGB888 for simplicity if we can convert, but RGB565 is
  // standard for embedded LCDs. Actually, for ESPHome Camera, we usually send
  // the raw image or a JPEG. Let's create an RGB565 buffer.

  if (this->image_buffer_.size() != 32 * 24 * 2) {
    this->image_buffer_.resize(32 * 24 * 2);
  }

  for (int i = 0; i < 768; i++) {
    float temp = this->mlx90640_to_[i];

    if (temp < min_temp)
      min_temp = temp;
    if (temp > max_temp)
      max_temp = temp;
    sum_temp += temp;
    sorted_temps.push_back(temp);
  }

  if (this->min_temperature_sensor_ != nullptr)
    this->min_temperature_sensor_->publish_state(min_temp);
  if (this->max_temperature_sensor_ != nullptr)
    this->max_temperature_sensor_->publish_state(max_temp);
  if (this->mean_temperature_sensor_ != nullptr)
    this->mean_temperature_sensor_->publish_state(sum_temp / 768.0f);

  if (this->median_temperature_sensor_ != nullptr) {
    std::sort(sorted_temps.begin(), sorted_temps.end());
    this->median_temperature_sensor_->publish_state(sorted_temps[384]);
  }

  // Configurable Range with Buffer (matching reference logic)
  const float min_scale = this->min_image_temp_ - 5.0f;
  const float effective_max = this->max_image_temp_ + 5.0f;
  const float scale_range = effective_max - min_scale;

  for (int y = 0; y < 24; y++) {
    for (int x = 0; x < 32; x++) {
      int i = y * 32 + x;
      float temp = this->mlx90640_to_[i];

      // Handle Sensor Errors/Saturation
      if (std::isnan(temp) || std::isinf(temp) || temp < -40.0f) {
        temp = effective_max; // Force to Max Hot on error
      }

      // Clamp to range
      if (temp < min_scale)
        temp = min_scale;
      if (temp > effective_max)
        temp = effective_max;

      // Map to 0-255 using the buffered range
      uint8_t index = (uint8_t)((temp - min_scale) / scale_range * 255.0f);

      // Lookup 565 Color
      uint16_t color = camColors[index];

      // Debug: Log center pixel details occasionally
      // i = 384 is approx center (12 * 32 = 384)
      if (i == 384) {
        // Log every ~2 seconds (every 4th frame at 2Hz)
        static int log_skipper = 0;
        if (log_skipper++ % 4 == 0) {
          ESP_LOGD(TAG,
                   "Center Pixel (index 384): Temp=%.2f C, MappedIndex=%d, "
                   "Color=0x%04X [MinScale=%.2f, MaxScale=%.2f]",
                   this->mlx90640_to_[i], index, color, min_scale,
                   effective_max);
        }
      }

      // Store in buffer (RGB565 Big Endian)
      this->image_buffer_[i * 2] = (uint8_t)(color >> 8);
      this->image_buffer_[i * 2 + 1] = (uint8_t)(color & 0xFF);
    }
  }
}

void MLX90640Component::set_refresh_rate_hw_() {
  // Basic mapping, needs checking against datasheet
  // 0x00: 0.5Hz, 0x01: 1Hz, 0x02: 2Hz, 0x03: 4Hz, 0x04: 8Hz, 0x05: 16Hz...
  int rate_code = 2; // Default 2Hz
  if (this->refresh_rate_ <= 1)
    rate_code = 1;
  else if (this->refresh_rate_ <= 2)
    rate_code = 2;
  else if (this->refresh_rate_ <= 4)
    rate_code = 3;
  else if (this->refresh_rate_ <= 8)
    rate_code = 4;
  else
    rate_code = 5; // 16Hz max typically for ESP

  MLX90640_SetRefreshRate(this->address_, rate_code);
}

void MLX90640Component::get_image_data(std::vector<uint8_t> &data) {
  std::lock_guard<std::mutex> lock(this->image_lock_);
  data = this->image_buffer_;
}

#ifdef USE_MLX90640_CAMERA
// Camera Implementation
void MLX90640Camera::setup() {
  // Nothing special needed, parent handles hardware
}

void MLX90640Camera::loop() {
  // Nothing special needed
}

void MLX90640Camera::request_image(camera::CameraRequester requester) {
  if (this->parent_ == nullptr)
    return;

  std::vector<uint8_t> data;
  this->parent_->get_image_data(data);

  auto image = std::make_unique<camera::CameraImage>(std::move(data), 32, 24);
  this->callback_manager_.call(std::move(image), requester);
}
#endif

#ifdef USE_MLX90640_WEB_SERVER
#include <esp_http_server.h>

esp_err_t mlx90640_web_server_handler(httpd_req_t *req) {
  // Retrieve the component instance from user_ctx
  MLX90640Component *component = (MLX90640Component *)req->user_ctx;

  // Create BMP header and data
  // 32x24 pixels, 3 bytes per pixel (RGB888) = 2304 bytes
  // Header = 54 bytes
  const int width = 32;
  const int height = 24;
  const int row_padded = (width * 3 + 3) & (~3);
  const int data_size = row_padded * height;
  const int file_size = 54 + data_size;

  // Use global buffer to persist data for send
  if (g_bmp_buffer.size() != file_size) {
    g_bmp_buffer.resize(file_size);
  }
  uint8_t *buf = g_bmp_buffer.data();

  // BMP Header
  buf[0] = 'B';
  buf[1] = 'M'; // Signature
  buf[2] = (uint8_t)(file_size);
  buf[3] = (uint8_t)(file_size >> 8);
  buf[4] = (uint8_t)(file_size >> 16);
  buf[5] = (uint8_t)(file_size >> 24); // File size
  buf[10] = 54;
  buf[11] = 0;
  buf[12] = 0;
  buf[13] = 0; // Offset to data

  // DIB Header
  buf[14] = 40;
  buf[15] = 0;
  buf[16] = 0;
  buf[17] = 0; // Header size
  buf[18] = width;
  buf[19] = 0;
  buf[20] = 0;
  buf[21] = 0; // Width
  buf[22] = height;
  buf[23] = 0;
  buf[24] = 0;
  buf[25] = 0; // Height (positive bottom-up)

  buf[26] = 1;
  buf[27] = 0; // Planes
  buf[28] = 24;
  buf[29] = 0; // Bits per pixel
  buf[30] = 0;
  buf[31] = 0;
  buf[32] = 0;
  buf[33] = 0; // Compression
  buf[34] = (uint8_t)(data_size);
  buf[35] = (uint8_t)(data_size >> 8);
  buf[36] = (uint8_t)(data_size >> 16);
  buf[37] = (uint8_t)(data_size >> 24); // Image size

  // Construct Data (Convert RGB565 buffer to RGB888 and flip Y for BMP
  // bottom-up)
  std::vector<uint8_t> rgb565_data;
  component->get_image_data(rgb565_data);

  // Safety check
  if (rgb565_data.size() < width * height * 2) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }

  uint8_t *p_data = buf + 54;
  for (int y = height - 1; y >= 0; y--) { // BMP is bottom-up
    for (int x = 0; x < width; x++) {
      int i = (y * width + x) * 2;
      uint16_t color565 = (rgb565_data[i] << 8) | rgb565_data[i + 1];

      // RGB565 -> RGB888
      // RGB565 -> RGB888 (Reference: Chill-Division/M5Stack-ESPHome)
      // Uses optimized integer arithmetic: (x * 527 + 23) >> 6 approx x * 255 /
      // 31
      uint8_t r_565 = (color565 >> 11) & 0x1F;
      uint8_t g_565 = (color565 >> 5) & 0x3F;
      uint8_t b_565 = color565 & 0x1F;

      // Reference Math:
      // red = ((((color >> 11) & 0x1F) * 527) + 23) >> 6;
      // green = ((((color >> 5) & 0x3F) * 259) + 33) >> 6;
      // blue = (((color & 0x1F) * 527) + 23) >> 6;

      uint8_t r = (r_565 * 527 + 23) >> 6;
      uint8_t g = (g_565 * 259 + 33) >> 6;
      uint8_t b = (b_565 * 527 + 23) >> 6;

      // Clamp (though math shouldn't overflow theoretically for valid 5/6 bit
      // inputs)
      if (r > 255)
        r = 255;
      if (g > 255)
        g = 255;
      if (b > 255)
        b = 255;

      // BMP is BGR
      *p_data++ = b;
      *p_data++ = g;
      *p_data++ = r;
    }
    // Padding
    for (int p = 0; p < (row_padded - width * 3); p++) {
      *p_data++ = 0;
    }
  }

  httpd_resp_set_type(req, "image/bmp");
  // httpd_resp_send works with a single buffer.
  httpd_resp_send(req, (const char *)buf, file_size);
  return ESP_OK;
}
#endif

} // namespace mlx90640
} // namespace esphome

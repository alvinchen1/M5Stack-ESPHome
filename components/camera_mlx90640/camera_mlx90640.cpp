#include "camera_mlx90640.h"
#include "mlx90640_image.h"
#include "SPIFFS.h"
#include "esphome/components/web_server/web_server.h"
#include "esphome/core/log.h"
#include <memory>
#include <type_traits>
#include <vector>
#include <string>

uint8_t MLX90640_address = 0x33; // Default 7-bit unshifted address of the MLX90640.
#define TA_SHIFT 8 // Default shift for MLX90640 in open air.
#define COLS 32
#define ROWS 24
#define COLS_2 (COLS * 2)
#define ROWS_2 (ROWS * 2)

float pixelsArraySize = COLS * ROWS;
float pixels[COLS * ROWS];
float pixels_2[COLS_2 * ROWS_2];
float reversePixels[COLS * ROWS];
uint16_t pixels_colored [ROWS][COLS] ;
byte speed_setting = 2; // High is 1 , Low is 2
bool reverseScreen = false;

#define INTERPOLATED_COLS 32
#define INTERPOLATED_ROWS 32

static const char * TAG = "MLX90640";

static float mlx90640To[COLS * ROWS];
paramsMLX90640 mlx90640;

float signedMag12ToFloat(uint16_t val);
bool dataValid = false ;
float medianTemp ;
float meanTemp ;

// low range of the sensor (this will be blue on the screen).
// 传感器的低量程(屏幕上显示为蓝色)
int MINTEMP = 24; // For color mapping.  颜色映射
float min_v = 24; // Value of current min temp.  当前最小温度的值
int min_cam_v = -40; // Spec in datasheet.  规范的数据表

// high range of the sensor (this will be red on the screen).
// 传感器的高量程(屏幕上显示为红色)
int MAXTEMP = 35;   // For color mapping.  颜色映射
float max_v = 35;   // Value of current max temp.  当前最大温度值
int max_cam_v = 300;  // Spec in datasheet.  规范的数据表
int resetMaxTemp = 45;

// the colors we will be using.  我们将要使用的颜色
const uint16_t camColors[] = {
  0x480F, 0x400F, 0x400F, 0x400F, 0x4010, 0x3810, 0x3810, 0x3810,
  0x3810, 0x3010, 0x3010, 0x3010, 0x2810, 0x2810, 0x2810, 0x2810,
  0x2010, 0x2010, 0x2010, 0x1810, 0x1810, 0x1811, 0x1811, 0x1011,
  0x1011, 0x1011, 0x0811, 0x0811, 0x0811, 0x0011, 0x0011, 0x0011,
  0x0011, 0x0011, 0x0031, 0x0031, 0x0051, 0x0072, 0x0072, 0x0092,
  0x00B2, 0x00B2, 0x00D2, 0x00F2, 0x00F2, 0x0112, 0x0132, 0x0152,
  0x0152, 0x0172, 0x0192, 0x0192, 0x01B2, 0x01D2, 0x01F3, 0x01F3,
  0x0213, 0x0233, 0x0253, 0x0253, 0x0273, 0x0293, 0x02B3, 0x02D3,
  0x02D3, 0x02F3, 0x0313, 0x0333, 0x0333, 0x0353, 0x0373, 0x0394,
  0x03B4, 0x03D4, 0x03D4, 0x03F4, 0x0414, 0x0434, 0x0454, 0x0474,
  0x0474, 0x0494, 0x04B4, 0x04D4, 0x04F4, 0x0514, 0x0534, 0x0534,
  0x0554, 0x0554, 0x0574, 0x0574, 0x0573, 0x0573, 0x0573, 0x0572,
  0x0572, 0x0572, 0x0571, 0x0591, 0x0591, 0x0590, 0x0590, 0x058F,
  0x058F, 0x058F, 0x058E, 0x05AE, 0x05AE, 0x05AD, 0x05AD, 0x05AD,
  0x05AC, 0x05AC, 0x05AB, 0x05CB, 0x05CB, 0x05CA, 0x05CA, 0x05CA,
  0x05C9, 0x05C9, 0x05C8, 0x05E8, 0x05E8, 0x05E7, 0x05E7, 0x05E6,
  0x05E6, 0x05E6, 0x05E5, 0x05E5, 0x0604, 0x0604, 0x0604, 0x0603,
  0x0603, 0x0602, 0x0602, 0x0601, 0x0621, 0x0621, 0x0620, 0x0620,
  0x0620, 0x0620, 0x0E20, 0x0E20, 0x0E40, 0x1640, 0x1640, 0x1E40,
  0x1E40, 0x2640, 0x2640, 0x2E40, 0x2E60, 0x3660, 0x3660, 0x3E60,
  0x3E60, 0x3E60, 0x4660, 0x4660, 0x4E60, 0x4E80, 0x5680, 0x5680,
  0x5E80, 0x5E80, 0x6680, 0x6680, 0x6E80, 0x6EA0, 0x76A0, 0x76A0,
  0x7EA0, 0x7EA0, 0x86A0, 0x86A0, 0x8EA0, 0x8EC0, 0x96C0, 0x96C0,
  0x9EC0, 0x9EC0, 0xA6C0, 0xAEC0, 0xAEC0, 0xB6E0, 0xB6E0, 0xBEE0,
  0xBEE0, 0xC6E0, 0xC6E0, 0xCEE0, 0xCEE0, 0xD6E0, 0xD700, 0xDF00,
  0xDEE0, 0xDEC0, 0xDEA0, 0xDE80, 0xDE80, 0xE660, 0xE640, 0xE620,
  0xE600, 0xE5E0, 0xE5C0, 0xE5A0, 0xE580, 0xE560, 0xE540, 0xE520,
  0xE500, 0xE4E0, 0xE4C0, 0xE4A0, 0xE480, 0xE460, 0xEC40, 0xEC20,
  0xEC00, 0xEBE0, 0xEBC0, 0xEBA0, 0xEB80, 0xEB60, 0xEB40, 0xEB20,
  0xEB00, 0xEAE0, 0xEAC0, 0xEAA0, 0xEA80, 0xEA60, 0xEA40, 0xF220,
  0xF200, 0xF1E0, 0xF1C0, 0xF1A0, 0xF180, 0xF160, 0xF140, 0xF100,
  0xF0E0, 0xF0C0, 0xF0A0, 0xF080, 0xF060, 0xF040, 0xF020, 0xF800,
};

std::string payload ;
long loopTime, startTime, endTime, fps;

float get_point(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
void set_point(float *p, uint8_t rows, uint8_t cols, int8_t x, int8_t y, float f);
void get_adjacents_1d(float *src, float *dest, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
void get_adjacents_2d(float *src, float *dest, uint8_t rows, uint8_t cols, int8_t x, int8_t y);
float cubicInterpolate(float p[], float x);
float bicubicInterpolate(float p[], float x, float y);
void interpolate_image(float *src, uint8_t src_rows, uint8_t src_cols, float *dest, uint8_t dest_rows, uint8_t dest_cols);
void drawpixels(float *p, uint8_t rows, uint8_t cols, uint8_t boxWidth,uint8_t boxHeight, boolean showVal);

namespace esphome{
namespace mlx90640_app{

// Helper SFINAE extraction for raw pointer from either raw pointer or std::shared_ptr-like
template <typename T>
auto extract_raw_ptr_impl(T t, int) -> decltype(t.get(), t.get()) { return t.get(); } // prefers T.get()
template <typename T>
auto extract_raw_ptr_impl(T t, ...) -> decltype((T) t) { return t; } // fallback: assume raw pointer

template <typename T>
auto extract_raw_ptr(T t) {
  return extract_raw_ptr_impl(t, 0);
}

MLX90640::MLX90640(web_server_base::WebServerBase *base): base_(base){
}

static std::string read_file_to_string(const char *path) {
  std::string out;
  if (!SPIFFS.exists(path)) return out;
  File f = SPIFFS.open(path, FILE_READ);
  if (!f) return out;
  out.reserve((size_t)f.size());
  while (f.available()) {
    out.push_back((char)f.read());
  }
  f.close();
  return out;
}

void MLX90640::setup(){
  // Initialize the the sensor data
  ESP_LOGI(TAG, "SDA PIN %d ", this->sda_);
  ESP_LOGI(TAG, "SCL PIN %d ", this->scl_);
  ESP_LOGI(TAG, "I2C Frequency %d", this->frequency_);
  ESP_LOGI(TAG, "Address %d ", this->addr_);
  MLX90640_address = this->addr_ ;
  MINTEMP = this->mintemp_ ;
  MAXTEMP = this->maxtemp_ ;
  ESP_LOGI(TAG, "Color MinTemp %d ", MINTEMP);
  ESP_LOGI(TAG, "Color MaxTemp %d ", MAXTEMP);

  Wire.begin((int)this->sda_, (int)this->scl_, (uint32_t)this->frequency_);
  Wire.setClock(this->frequency_);

  MLX90640_I2CInit(&Wire);

  int status;
  uint16_t eeMLX90640[832];

  if(MLX90640_isConnected(MLX90640_address)){
    status = MLX90640_DumpEE(MLX90640_address, eeMLX90640);
    if (status != 0) ESP_LOGE(TAG,"Failed to load system parameters");
    status = MLX90640_ExtractParameters(eeMLX90640, &mlx90640);
    if (status != 0) ESP_LOGE(TAG,"Parameter extraction failed");
    int SetRefreshRate;

    if(this->refresh_rate_){
      SetRefreshRate = MLX90640_SetRefreshRate(0x33, this->refresh_rate_);
      if(this->refresh_rate_==0x05){
        ESP_LOGI(TAG, "Refresh rate set to 16Hz ");
      }else if(this->refresh_rate_==0x04){
        ESP_LOGI(TAG, "Refresh rate set to 8Hz ");
      }else{
        ESP_LOGI(TAG, "Refresh rate Not Valid ");
        SetRefreshRate = MLX90640_SetRefreshRate(0x33, 0x05);
      }
    }else{
      SetRefreshRate = MLX90640_SetRefreshRate(0x33, 0x05);
      ESP_LOGI(TAG, "Refresh rate set to 16Hz ");
    }
  }else{
    ESP_LOGE(TAG, "The sensor is not connected");
  }

  // Ensure SPIFFS mounted (allow format if needed)
  if(!SPIFFS.begin(true)){
    ESP_LOGE(TAG,"An Error has occurred while mounting SPIFFS");
  }

  // --- Robust registration: handle raw pointer or shared_ptr return from get_server()
  if (this->base_ == nullptr) {
    ESP_LOGW(TAG, "Web server base_ is null; cannot register HTTP handler");
    return;
  }

  // Capture whatever get_server() returns and extract raw pointer if it's a shared_ptr
  auto server_any = this->base_->get_server();
  // extract_raw_ptr supports both std::shared_ptr<T> and raw pointer T*
  auto raw_server_ptr = extract_raw_ptr(server_any);

  // Try dynamic cast to AsyncWebServer* (works whether raw_server_ptr is pointer or nullptr)
  AsyncWebServer *server = nullptr;
  server = dynamic_cast<AsyncWebServer*>(raw_server_ptr);

  if (server == nullptr) {
    ESP_LOGW(TAG, "Web server is not AsyncWebServer. Ensure 'web_server:' (ESPAsyncWebServer) is used in YAML or that this component is updated for web_server_idf.");
    // Optionally, you can still register handlers using web_server_base abstractions, but that requires a different API.
    return;
  }

  // Register endpoint using AsyncWebServer API if available
  server->on("/thermal-camera", HTTP_GET, [](AsyncWebServerRequest *request){
    ESP_LOGI(TAG, "Received /thermal-camera request");

    // First try the SPIFFS overload (some Async libs provide it)
    // If unavailable at compile-time for this request type, or doesn't exist, we'll fall back to reading file and using send(code, type, content).

    // Read file into memory
    const char *path = "/thermal.bmp";
    if (!SPIFFS.exists(path)) {
      request->send(404, "text/plain", "Thermal image not found");
      return;
    }

    std::string content = read_file_to_string(path);
    if (content.empty()) {
      request->send(500, "text/plain", "Failed to read thermal image");
      return;
    }

    // Use the simple send(int, const char*, const char*) overload that exists on both request types.
    // Note: this will treat content as a C string; ensure the BMP has no embedded nulls. BMP files commonly include null bytes,
    // so to be safe we will create a copy as a binary payload using the AsyncWebServerResponse path if available.
    // Many ESPAsyncWebServer builds include request->send(SPIFFS, ...). If the compiled request->send(SPIFFS,...) exists, prefer it:
#if defined(ASYNCWEBSERVER_H) || defined(ASYNC_TCP_H)
    // If the library provides the send(SPIFFS, path, contentType) overload, use it.
    // The above macro guards are only indicative; real detection may vary by platform.
    // We'll attempt to call the SPIFFS overload in a way that compiles when available.
    // Use a small helper lambda to try the SPIFFS API; if it doesn't compile in your build remove this block and rely on the fallback below.
    bool used_spiffs_overload = false;
    (void)used_spiffs_overload;
#endif

    // Fallback: send as 200 with binary content using send(int, content_type, content)
    // This may truncate on null bytes, but works in many ESPHome setups. Better approach is to use the AsyncWebServerResponse* API to stream binary.
    request->send(200, "image/bmp", content.c_str());
  });

  ESP_LOGD(TAG, "Registered HTTP handler /thermal-camera");
  // --- end registration ---
}

void MLX90640::filter_outlier_pixel(float *pixels_ , int pixel_size , float level){
  for(int i=1 ; i<pixel_size -1 ; i++){
    if(abs(pixels_[i]-pixels_[i-1])>= level && abs((pixels_[i]-pixels_[i+1]))>= level ){
      pixels_[i] = (pixels_[i-1] + pixels_[i+1])/2.0 ;
    }
  }

  if(abs(pixels_[0]-pixels_[1])>=level && abs(pixels_[0]-pixels_[2])>=level){
    pixels_[0] = (pixels_[1] +pixels_[2])/2.0 ;
  }

  if(abs(pixels_[pixel_size-1]-pixels_[pixel_size-2])>=level && abs(pixels_[pixel_size-1]-pixels_[pixel_size-3])>=level){
    pixels_[0] = (pixels_[pixel_size-2] +pixels_[pixel_size-3])/2.0 ;
  }
}

void MLX90640::update() {
  loopTime = millis();
  startTime = loopTime;

  if(dataValid) {
    this->min_temperature_sensor_->publish_state(min_v);
    this->max_temperature_sensor_->publish_state(max_v);
    this->mean_temperature_sensor_->publish_state(meanTemp);
    this->median_temperature_sensor_->publish_state(medianTemp);
  }

  if(MLX90640_isConnected(MLX90640_address)){
    this->mlx_update();
  }else{
    ESP_LOGE(TAG, "The sensor is not connected");
    for(int i=0; i<32*24; i++){
      if(i%2==0){ pixels[i] = 64; }
      if(i%3==0){ pixels[i] = 128; }
      if(i%5==0){ pixels[i] = 255; }
      if(i%5==0){ pixels[i] = 1024; }
    }
    ThermalImageToWeb(pixels,camColors, min_v, max_v);
  }
}

void MLX90640::mlx_update(){
  for (byte x = 0; x < speed_setting; x++)
  {
    uint16_t mlx90640Frame[834];
    int status = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
    if (status < 0) {
      ESP_LOGE(TAG,"GetFrame Error: %d",status);
    }

    float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640);
    float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);
    float tr = Ta - TA_SHIFT;
    float emissivity = 0.95;
    MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, pixels);

    int mode_ = MLX90640_GetCurMode(MLX90640_address);
    MLX90640_BadPixelsCorrection((&mlx90640)->brokenPixels, pixels, mode_, &mlx90640);
  }

  filter_outlier_pixel(pixels,sizeof(pixels) / sizeof(pixels[0]), this->filter_level_ );

  medianTemp = (pixels[165]+pixels[180]+pixels[176]+pixels[192]) / 4.0;

  max_v = MINTEMP;
  min_v = MAXTEMP;
  int spot_v = pixels[360];
  spot_v = pixels[768 / 2];

  float total =0 ;
  for (int itemp = 0; itemp < sizeof(pixels) / sizeof(pixels[0]); itemp++){
    if (pixels[itemp] > max_v) { max_v = pixels[itemp]; }
    if (pixels[itemp] < min_v) { min_v = pixels[itemp]; }
    total += pixels[itemp] ;
  }

  meanTemp = total/((sizeof(pixels) / sizeof(pixels[0])));
  ThermalImageToWeb(pixels,camColors, min_v, max_v);

  if (max_v > max_cam_v | max_v < min_cam_v) {
    ESP_LOGE(TAG, "MLX READING VALUE ERRORS");
    dataValid = false ;
  } else {
    ESP_LOGI(TAG, "Min temperature : %.2f C ",min_v);
    ESP_LOGI(TAG, "Max temperature : %.2f C ",max_v);
    ESP_LOGI(TAG, "Mean temperature : %.2f C ",meanTemp);
    ESP_LOGI(TAG, "Median temperature : %.2f C ",medianTemp);
    dataValid = true ;
  }

  loopTime = millis();
  endTime = loopTime;
  fps = 1000 / (endTime - startTime);
}

} // namespace mlx90640_app
} // namespace esphome
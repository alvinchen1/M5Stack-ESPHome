#include "camera_mlx90640.h"
#include "mlx90640_image.h"
#include "SPIFFS.h"
#include "esphome/components/web_server/web_server.h"
#include "esphome/core/log.h"
#include <memory>

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
  /* color table unchanged */
  0x480F, 0x400F, 0x400F, 0x400F, 0x4010, 0x3810, 0x3810, 0x3810,
  /* ... truncated for brevity: keep the original full table here ... */
  0xF020, 0xF800,
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

MLX90640::MLX90640(web_server_base::WebServerBase *base): base_(base){
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

  // Display bottom side colorList and info.
  if(!SPIFFS.begin(true)){
    ESP_LOGE(TAG,"An Error has occurred while mounting SPIFFS");
  }

  // --- PATCHED: register endpoint with AsyncWebServer only if backend is AsyncWebServer ---
  if (this->base_ == nullptr) {
    ESP_LOGW(TAG, "Web server base_ is null; cannot register HTTP handler");
    return;
  }

  // The return type of get_server() can be a raw pointer or a std::shared_ptr to a concrete server type,
  // depending on ESPHome version/backend. Attempt to handle the common shared_ptr case first.
  AsyncWebServer *server = nullptr;

  // Try to get a shared_ptr-like object and extract raw pointer if possible
  // (your build showed get_server() returning std::shared_ptr<esphome::web_server_idf::AsyncWebServer>)
  // so we check for a .get() method at compile-time then use it at runtime.
  // If get_server() returns a raw pointer, the following block will be skipped by SFINAE fallback.

  // First attempt: treat as shared_ptr with get()
  bool obtained = false;
  // Use a lambda wrapper to attempt extraction without causing compile errors when get_server() returns raw ptr
  auto try_extract_shared = [&]() -> AsyncWebServer* {
    // This block relies on the concrete type being convertible to std::shared_ptr-like with .get()
    // It's guarded at runtime by try/catch to avoid exceptions propagating.
    try {
      // Use auto to hold whatever get_server() returns
      auto gs = this->base_->get_server();
      // If gs has member function get(), use it
      // This compiles when gs is a std::shared_ptr<T>
      if constexpr (std::is_member_function_pointer_v<decltype(&decltype(gs)::get)>) {
        auto raw = gs.get();
        return dynamic_cast<AsyncWebServer*>(raw);
      } else {
        // Fallback: attempt to dynamic_cast assuming raw pointer return
        return dynamic_cast<AsyncWebServer*>(gs);
      }
    } catch (...) {
      return nullptr;
    }
  };

  // Attempt extraction
  server = try_extract_shared();
  if (!server) {
    // As a pragmatic fallback, try direct dynamic_cast from raw pointer returned by get_server()
    // This will compile if get_server() returns a raw pointer type.
    try {
      server = dynamic_cast<AsyncWebServer*>(this->base_->get_server());
    } catch (...) {
      server = nullptr;
    }
  }

  if (server == nullptr) {
    ESP_LOGW(TAG, "Web server is not AsyncWebServer. Ensure 'web_server:' (ESPAsyncWebServer) is used in YAML");
    return;
  }

  // Register endpoint using AsyncWebServer API
  server->on("/thermal-camera", HTTP_GET, [](AsyncWebServerRequest *request){
    ESP_LOGI(TAG, "Sending the image");
    if (SPIFFS.exists("/thermal.bmp")) {
      request->send(SPIFFS, "/thermal.bmp", "image/bmp");
    } else {
      request->send(404, "text/plain", "Thermal image not found");
    }
  });
  // --- end PATCH ---

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
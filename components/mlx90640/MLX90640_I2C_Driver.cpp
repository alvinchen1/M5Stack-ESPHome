#include "MLX90640_I2C_Driver.h"
#include "esphome/core/log.h"

static esphome::i2c::I2CDevice *global_mlx_device = nullptr;
static const char *const TAG = "mlx90640_driver";

void MLX90640_SetDevice(esphome::i2c::I2CDevice *dev) {
  global_mlx_device = dev;
}

// Read a number of words from startAddress. Store into Data array.
// Returns 0 if successful, -1 if error
int MLX90640_I2CRead(uint8_t _deviceAddress, unsigned int startAddress,
                     unsigned int nWordsRead, uint16_t *data) {
  if (global_mlx_device == nullptr)
    return -1;

  uint8_t cmd[2];
  cmd[0] = startAddress >> 8;
  cmd[1] = startAddress & 0xFF;

  if (global_mlx_device->write(cmd, 2) != esphome::i2c::ERROR_OK) {
    return -1;
  }

  uint16_t bytesRemaining = nWordsRead * 2;
  // Use vector to avoid VLA
  std::vector<uint8_t> buf(bytesRemaining);

  if (global_mlx_device->read(buf.data(), bytesRemaining) !=
      esphome::i2c::ERROR_OK) {
    return -1;
  }

  for (int i = 0; i < nWordsRead; i++) {
    data[i] = (buf[i * 2] << 8) | buf[i * 2 + 1];
  }

  return 0; // Success
}

// Set I2C Freq, in kHz
void MLX90640_I2CFreqSet(int freq) {
  // Managed by ESPHome I2C component
}

// Write two bytes to a two byte address
int MLX90640_I2CWrite(uint8_t _deviceAddress, unsigned int writeAddress,
                      uint16_t data) {
  if (global_mlx_device == nullptr)
    return -1;

  uint8_t cmd[4];
  cmd[0] = writeAddress >> 8;
  cmd[1] = writeAddress & 0xFF;
  cmd[2] = data >> 8;
  cmd[3] = data & 0xFF;

  if (global_mlx_device->write(cmd, 4) != esphome::i2c::ERROR_OK) {
    return -1;
  }

  return 0; // Success
}

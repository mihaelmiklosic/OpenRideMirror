#pragma once

#include <Arduino.h>
#include <driver/i2c_master.h>

namespace orm {

class I2cBus {
 public:
  I2cBus(int sclPin, int sdaPin, int port);
  ~I2cBus();
  i2c_master_bus_handle_t handle() const { return handle_; }

 private:
  i2c_master_bus_handle_t handle_ = nullptr;
};

class Board {
 public:
  bool begin(I2cBus &bus);
  bool readEnvironment(float &temperatureC, float &humidityPercent);
  bool setClock(uint8_t hour, uint8_t minute, uint8_t second);
  bool readClock(uint8_t &hour, uint8_t &minute, uint8_t &second);

 private:
  static uint8_t crc8(const uint8_t *data, size_t length);
  static uint8_t toBcd(uint8_t value);
  static uint8_t fromBcd(uint8_t value);

  i2c_master_dev_handle_t shtc3_ = nullptr;
  i2c_master_dev_handle_t rtc_ = nullptr;
};

}  // namespace orm

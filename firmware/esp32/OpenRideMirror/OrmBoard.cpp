#include "OrmBoard.h"

namespace orm {

I2cBus::I2cBus(int sclPin, int sdaPin, int port) {
  i2c_master_bus_config_t config = {};
  config.clk_source = I2C_CLK_SRC_DEFAULT;
  config.i2c_port = static_cast<i2c_port_t>(port);
  config.scl_io_num = static_cast<gpio_num_t>(sclPin);
  config.sda_io_num = static_cast<gpio_num_t>(sdaPin);
  config.glitch_ignore_cnt = 7;
  config.flags.enable_internal_pullup = true;
  ESP_ERROR_CHECK(i2c_new_master_bus(&config, &handle_));
}

I2cBus::~I2cBus() {
  if (handle_ != nullptr) i2c_del_master_bus(handle_);
}

static bool addDevice(i2c_master_bus_handle_t bus, uint8_t address,
                      i2c_master_dev_handle_t &device) {
  i2c_device_config_t config = {};
  config.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  config.device_address = address;
  config.scl_speed_hz = 400000;
  return i2c_master_bus_add_device(bus, &config, &device) == ESP_OK;
}

bool Board::begin(I2cBus &bus) {
  const bool sensorOk = addDevice(bus.handle(), 0x70, shtc3_);
  const bool rtcOk = addDevice(bus.handle(), 0x51, rtc_);
  return sensorOk || rtcOk;
}

uint8_t Board::crc8(const uint8_t *data, size_t length) {
  uint8_t crc = 0xff;
  for (size_t index = 0; index < length; ++index) {
    crc ^= data[index];
    for (uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x31)
                         : static_cast<uint8_t>(crc << 1);
    }
  }
  return crc;
}

bool Board::readEnvironment(float &temperatureC, float &humidityPercent) {
  if (shtc3_ == nullptr) return false;
  const uint8_t wake[] = {0x35, 0x17};
  const uint8_t measure[] = {0x7c, 0xa2};
  const uint8_t sleep[] = {0xb0, 0x98};
  uint8_t result[6] = {};
  if (i2c_master_transmit(shtc3_, wake, sizeof(wake), 100) != ESP_OK) return false;
  delay(1);
  if (i2c_master_transmit(shtc3_, measure, sizeof(measure), 100) != ESP_OK) return false;
  delay(15);
  const bool ok = i2c_master_receive(shtc3_, result, sizeof(result), 100) == ESP_OK;
  i2c_master_transmit(shtc3_, sleep, sizeof(sleep), 100);
  if (!ok || crc8(result, 2) != result[2] || crc8(result + 3, 2) != result[5]) return false;
  const uint16_t rawTemperature = (static_cast<uint16_t>(result[0]) << 8) | result[1];
  const uint16_t rawHumidity = (static_cast<uint16_t>(result[3]) << 8) | result[4];
  temperatureC = -45.0f + 175.0f * rawTemperature / 65535.0f;
  humidityPercent = 100.0f * rawHumidity / 65535.0f;
  return true;
}

uint8_t Board::toBcd(uint8_t value) {
  return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}

uint8_t Board::fromBcd(uint8_t value) {
  return static_cast<uint8_t>(((value >> 4) * 10) + (value & 0x0f));
}

bool Board::setClock(uint8_t hour, uint8_t minute, uint8_t second) {
  if (rtc_ == nullptr || hour > 23 || minute > 59 || second > 59) return false;
  const uint8_t registers[] = {0x04, toBcd(second), toBcd(minute), toBcd(hour)};
  return i2c_master_transmit(rtc_, registers, sizeof(registers), 100) == ESP_OK;
}

bool Board::readClock(uint8_t &hour, uint8_t &minute, uint8_t &second) {
  if (rtc_ == nullptr) return false;
  uint8_t start = 0x04;
  uint8_t data[3] = {};
  if (i2c_master_transmit_receive(rtc_, &start, 1, data, sizeof(data), 100) != ESP_OK) return false;
  second = fromBcd(data[0] & 0x7f);
  minute = fromBcd(data[1] & 0x7f);
  hour = fromBcd(data[2] & 0x3f);
  return hour < 24 && minute < 60 && second < 60;
}

}  // namespace orm

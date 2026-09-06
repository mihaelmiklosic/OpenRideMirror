# Reference hardware

## Exact board to buy

OpenRideMirror v0.1 targets this exact integrated development board:

- **Manufacturer:** Waveshare
- **Product name:** ESP32-S3-RLCD-4.2
- **Product page:** [waveshare.com/product/esp32-s3-rlcd-4.2.htm](https://www.waveshare.com/product/esp32-s3-rlcd-4.2.htm)
- **Documentation:** [docs.waveshare.com/ESP32-S3-RLCD-4.2](https://docs.waveshare.com/ESP32-S3-RLCD-4.2)
- **SKU 33298:** ESP32-S3-RLCD-4.2, supplied with an 18650 battery
- **SKU 33507:** ESP32-S3-RLCD-4.2-EN, supplied without the 18650 battery

Both variants use the same target electronics for this project; the difference documented by Waveshare is whether the optional 18650 battery is included. USB-C power is sufficient for building and testing.

The identifying features are:

- the screen and ESP32-S3 are part of one development board;
- the board marking/product name contains **ESP32-S3-RLCD-4.2**;
- the module is **ESP32-S3-WROOM-1-N16R8**;
- the display is a **4.2-inch 400 × 300 reflective LCD (RLCD)** with no backlight; OpenRideMirror uses it as a 300 × 400 portrait canvas;
- Waveshare lists 16 MB flash and 8 MB PSRAM;
- it has an onboard SHTC3 sensor, PCF85063 RTC, USB-C, microSD slot, speaker connector, dual microphones and an 18650 holder.

The [official Waveshare Arduino guide](https://docs.waveshare.com/ESP32-S3-RLCD-4.2/Arduino) says this board requires Arduino ESP32 core 3.3.0 or newer; ORM pins the physically tested 3.3.11 release.

### Similar products that are not drop-in compatible

This board is not the same as:

- a Waveshare **4.2-inch e-Paper** display or module;
- a standalone **ESP32 e-Paper Driver Board**;
- another Waveshare ESP32-S3 LCD/touch-LCD board;
- a generic ESP32-S3 board connected to a separate screen.

The RLCD has an e-paper-like reflective appearance, but it is still an LCD and refreshes differently. OpenRideMirror’s custom ST7305 driver, framebuffer mapping, physical pins, sensor setup and Arduino profile are specific to the ESP32-S3-RLCD-4.2. Those other products need a port.

## Tested specification

On the reference board, OpenRideMirror uses:

- ESP32-S3 revision 0.2, 240 MHz
- 8 MB OPI PSRAM
- 400 × 300 monochrome ST7305 reflective LCD
- onboard SHTC3 temperature/humidity sensor
- onboard RTC used after receiving the watch clock

The logical UI is portrait, 300 × 400 pixels. The known-good display connections are:

| Signal | GPIO |
|---|---:|
| LCD SCK | 11 |
| LCD MOSI | 12 |
| LCD DC | 5 |
| LCD CS | 40 |
| LCD reset | 41 |
| I²C SDA | 14 |
| I²C SCL | 13 |

## Exact Arduino profile

Use board `ESP32S3 Dev Module` and this FQBN:

```text
esp32:esp32:esp32s3:CDCOnBoot=cdc,CPUFreq=240,FlashMode=qio,FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,UploadMode=default,UploadSpeed=921600,USBMode=hwcdc
```

Equivalent options are USB CDC enabled, 240 MHz CPU, QIO flash, 16 MB flash, 3 MB app/9 MB FATFS partition, OPI PSRAM, hardware CDC/JTAG and 921600 upload speed.

## Sleeping between rides

The receiver decides when it may sleep in `OrmPowerState.h`, deliberately free
of Arduino and ESP-IDF headers so the rules can be compiled and exercised on a
workstation — `orm test` does exactly that.

Two rules, and the second matters more than it looks:

- sleep after five minutes with no movement;
- **never** sleep while the watch is connected with its activity timer running.

Stopping at a light, adjusting a shoe or waiting to cross all leave the bike
still while the ride is very much in progress, and that is precisely when a
rider looks at the screen. Waking is not free: a full BLE reconnection measured
6.4 s against a real Fenix 8, before board boot and the first RLCD refresh. The
saving is meant for a bike parked in a garage, not for traffic lights.

Nothing sleeps yet. The only wake source planned is the motion interrupt from an
MPU6050, which is not fitted; entering deep sleep without it would leave the
board off until a manual reset. Until then the decision is computed and logged
over serial, and telemetry showing real speed stands in for the accelerometer.

## Power and mounting

The battery is optional at purchase. If using one, follow Waveshare’s polarity and charging guidance for the exact board; the software does not currently report receiver battery percentage. Design an enclosure and power system around the actual board, battery and charger you choose. Confirm weather resistance, connector strain relief, screen visibility, bike clearances and safe mounting before road use. A prototype mount is not a safety-rated product.

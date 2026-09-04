# Reference hardware

## Tested board

OpenRideMirror v0.1 targets the Waveshare ESP32-S3-RLCD-4.2:

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

## Power and mounting

The software does not currently report receiver battery percentage. Design an enclosure and power system around the actual board, battery and charger you choose. Confirm weather resistance, connector strain relief, screen visibility, bike clearances and safe mounting before road use. A prototype mount is not a safety-rated product.

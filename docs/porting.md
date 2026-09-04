# Porting OpenRideMirror

## Another ESP32 board

The BLE protocol and application state are portable across ESP32 variants supported by the Arduino ESP32 core. The board is not the hard part; the display and peripherals are.

To support another device, provide:

1. a display driver that exposes the drawing operations used by the layout, or adapt the renderer to another graphics library;
2. correct width, height, rotation and buffer mapping;
3. board pin definitions and initialization;
4. an optional environment-sensor adapter, returning unknown values if absent;
5. a new, explicit build profile instead of silently reusing the Waveshare FQBN;
6. physical tests for boot, full refresh, BLE coexistence, memory use and USB upload.

The current source directly uses U8g2 and the custom ST7305 driver, so another random ESP32 display will not compile unchanged. Keeping the ORM BLE UUIDs and packet parser allows interoperability with the Garmin app.

## Another Garmin watch

First verify the product in the installed Connect IQ SDK and that it supports the manifest API level, Bluetooth Low Energy, Positioning and data-field activity information. Then:

1. add the exact product ID to `manifest.xml`;
2. compile using `./orm build garmin --device <id>` after extending the supported-device list;
3. test discovery, connection, field availability and update cadence on hardware;
4. document the result as tested or compile-only.

Do not claim broad Garmin compatibility from a successful simulator build alone.

## Another screen size

Treat this as a new layout target. The current 300 × 400 geometry, bitmap fonts and map viewport are intentionally pixel-specific. Keep the transport/parser shared, but isolate display constants and layout code behind a target-specific renderer.

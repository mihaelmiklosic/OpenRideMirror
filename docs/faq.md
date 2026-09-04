# Frequently asked questions

## Is OpenRideMirror a standalone bike computer?

No. Version 0.1 is deliberately a Garmin companion display. The reference ESP32 board has no GPS receiver and does not record the workout. The Garmin watch remains the activity recorder, GPS and sensor hub.

Without an active Garmin activity containing the **ORM Live** data field, the display has no live ride telemetry. It can still read ambient temperature and show locally saved ride summaries.

## Does it need a phone or internet while riding?

No. The watch sends data directly to the ESP32 over Bluetooth Low Energy. A phone is not part of the connection, and the map is stored locally. Internet access is used only during development or when generating a new OpenStreetMap extract.

## Which data comes from Garmin?

ORM protocol v1 carries:

- activity state and sport type;
- current, average and maximum speed;
- distance and elapsed activity time;
- current, average and maximum heart rate plus HR zone;
- GPS position, quality, heading and altitude;
- total ascent and calories;
- the watch's local time.

Ambient temperature and humidity are read from the Waveshare board's SHTC3 sensor. The current UI displays temperature. Ride-state classification, map drawing and UI animation run on the ESP32.

## How is the map displayed?

ORM does not download map tiles during a ride. A small OpenStreetMap area is preprocessed into simplified roads, street labels and a coarse greenery mask. The generator clips and quantizes that vector data into C++ headers stored in ESP32 flash.

Garmin supplies the current GPS coordinates and heading. The ESP32 uses that position to draw nearby roads, labels and the ridden track. The [browser map builder](../web-demo/map-builder.html) produces a ready-to-use map pack. Much larger map databases could use external storage, but SD-card map loading is not part of v0.1.

## Is this Garmin screen mirroring?

No. A custom Connect IQ data field reads values Garmin exposes to an active activity and encodes them into fixed ORM packets. The ESP32 runs custom firmware and renders its own interface. It does not display the Garmin UI or run other Connect IQ apps.

## Does data travel both ways?

The current application-level telemetry is one way: Garmin → display. Garmin acts as the BLE central and writes data to the ESP32 GATT service. The ESP32 does not send workouts back to Garmin or Strava.

An ESP32 with external GPS and sensors could be developed into a standalone recorder or BLE sensor, but that would be a separate architecture and is not implemented here.

## How are the devices paired?

There is no hardcoded MAC address or manual address entry. The display advertises the exact name `ORM` and the project service UUID. The Garmin data field scans for both and connects to one matching receiver. A replacement compatible board can therefore have a different BLE address without requiring a new Garmin build.

Version 0.1 does not use bonding or encryption. See [Security](../SECURITY.md) for the tradeoff.

## Can it run on another ESP32, M5Core or OLED display?

The protocol and application logic are portable, but the included firmware is not a generic screen binary. It targets the exact Waveshare ESP32-S3-RLCD-4.2, its pins, SHTC3 sensor and ST7305 display driver.

Another ESP32 display can use the same Garmin data field and ORM protocol after someone adds the correct board definition, display driver and layout adapter. See [Porting](porting.md).

## Is this e-ink?

No. The reference device uses a 4.2-inch monochrome reflective LCD with an ST7305 controller. It remains readable in direct light but refreshes quickly enough for live telemetry and animation. A MIP version has not been tested.

## Could it become fully autonomous and navigate a GPX route?

Technically yes, but it would need its own GPS, activity recorder, route engine, storage and sensor integration. OpenRideMirror intentionally uses the Garmin already on the rider's wrist rather than duplicating those components.

## Does it work for walking?

Cycling is the primary interface, but the protocol includes walking, hiking and other sport identifiers. GPS and available activity metrics can stream whenever **ORM Live** is installed in that activity profile and Garmin exposes the values to the data field.

## Is it waterproof?

The current development board and temporary mount are not weatherproof. A sealed enclosure and proper bike mounting system are future hardware work.

## What is the delay?

The prototype has shown roughly one to two seconds of observed delay and feels live for this interface. The transport always prefers the newest packet and discards stale queued values. Latency has not yet been formally benchmarked and can depend on Garmin's data-field scheduling.

## Is the source available, and was AI used?

The repository publishes source rather than prebuilt Garmin `.prg` or ESP32 firmware files. AI-assisted development was used extensively under the maintainer's direction; the design, integration and physical hardware behavior were reviewed and tested by the maintainer. See [AI usage](../AI_USAGE.md).

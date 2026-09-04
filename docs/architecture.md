# Architecture

OpenRideMirror has four deliberately separate layers.

## Garmin data field

`garmin/OpenRideMirror` is a Connect IQ data field embedded in a normal Garmin activity screen. Its compute callback reads the activity information Garmin exposes to data fields. The BLE transport scans for the exact device name and service UUID, refuses an ambiguous multi-display match, connects, and sends the newest packet of each type in round-robin order.

It does not discover the ESP32 through a hardcoded MAC address and it does not require a saved BLE bond. Replacing the receiver therefore does not require rebuilding the watch app.

## ORM protocol

`protocol/orm-protocol.json` is the source of truth for the 20-byte little-endian packets. Protocol v1 has activity, GPS and extended-statistics packets. Golden byte fixtures protect encoding compatibility across Python, Garmin and ESP code.

The transport is optimized for freshness: only the newest pending packet of each type is retained. Old telemetry is discarded instead of building an ever-growing queue.

## ESP32 receiver

The ESP32 is the BLE peripheral/GATT server. It validates packet size, version, type and plausible value ranges before updating a snapshot consumed by the renderer. Display rendering and BLE callbacks are separated by a small critical section.

The firmware adds two local responsibilities:

- reading temperature and humidity from the board SHTC3 sensor;
- persisting up to five compact ride summaries in ESP32 preferences with a versioned record and checksum.

The current UI shows temperature but not humidity. Humidity is retained in firmware for future layouts.

## Offline map compiler

The map tool accepts a bundled synthetic source or a small OpenStreetMap extract from Overpass. It classifies roads, labels and green polygons, clips them into geographic tiles, quantizes coordinates, and emits C++ headers. Firmware remains entirely offline while riding.

Generated personal maps live in `.orm/generated/map` and are copied into `.orm/staging/OpenRideMirror` only during a build. The checked-in sample headers are never overwritten.

## Developer surfaces

- `orm simulate web` serves a deterministic browser UI recreation.
- `orm simulate ble` acts as a Garmin-like BLE central for receiver development.
- `orm build` stages and builds source without polluting checked-in directories.
- `orm release check` enforces the public-source boundary.

# Garmin setup

OpenRideMirror is a Connect IQ **data field**, not a standalone watch app. It runs only while included on a data page of the activity profile you start.

## Current targets

The manifest includes:

- Fenix 7, 7S and 7X
- Fenix 7 Pro, 7S Pro and 7X Pro
- the no-Wi-Fi variants present in the Connect IQ SDK

All listed targets compile with Connect IQ SDK 9.2.0. Fenix 7 is the only physically tested target so far. Compatibility with another Garmin family requires adding its product ID and verifying that its Connect IQ version, BLE permissions and exposed activity fields behave correctly.

## Build

```sh
orm build garmin --device fenix7
```

To compile every manifest target:

```sh
orm build garmin --all
```

The CLI creates a local developer key in `.orm/keys/` if needed and places `.prg` output in `.orm/build/garmin/`. Neither is committed.

## Sideload and activate

1. Connect the watch over USB.
2. Copy the matching locally built `.prg` into the watch `GARMIN/APPS` directory.
3. Safely eject and let the watch process the file. The watch can move or rename it internally; the original filename may no longer remain visible.
4. Edit the desired activity profile, add a data screen/data field, and select **ORM Live**.
5. Start that activity while the ESP32 live firmware is powered.

Installing a new `.prg` may silently replace the app with the same Connect IQ application ID. The visible version and behavior are better checks than whether the watch displayed an update prompt.

Walking can provide GPS too: position is not cycling-specific. Availability and refresh depend on the activity profile, GPS lock and which fields Garmin exposes at that moment.

## What comes from where

From Garmin: activity/timer state, sport, current and average speed, distance, current/average/max heart rate, HR zone, GPS position, altitude, heading, ascent, calories and clock.

From the ESP32 board: ambient temperature and humidity. The current dashboard shows temperature.

Cadence and cycling power are not claimed by v0.1. Their protocol bytes are reserved and sent as unknown because they require appropriate external sensor data.

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
./orm garmin fenix7
```

To compile every manifest target:

```sh
./orm build garmin --all
```

The CLI creates a local developer key in `.orm/keys/` if needed and places `.prg` output in `.orm/build/garmin/`. Neither is committed.

## Sideload and activate

1. Connect the watch over USB.
2. Copy the matching locally built `.prg` into the watch `GARMIN/APPS` directory.
3. Safely eject and let the watch process the file. The watch can move or rename it internally; the original filename may no longer remain visible.
4. On the watch, press **START**, highlight the desired activity, hold **MENU**, and open its **Data Screens** settings.
5. Add or edit a field and select **Connect IQ Fields → ORM Live**, then save the activity profile.
6. Power the ESP32 with live firmware installed.
7. Open that Garmin activity, wait for GPS and press **START** again to begin recording.

Installing a new `.prg` may silently replace the app with the same Connect IQ application ID. The visible version and behavior are better checks than whether the watch displayed an update prompt.

Walking can provide GPS too: position is not cycling-specific. Availability and refresh depend on the activity profile, GPS lock and which fields Garmin exposes at that moment.

ORM Live must be added separately to Bike, Walk or any other activity you want to use. It is not launched from the watch's app list, and merely installing the `.prg` does not make it run in every activity. The complete first-use sequence is in [Getting started](getting-started.md#7-run-the-first-live-test).

## What comes from where

From Garmin: activity/timer state, sport, current and average speed, distance, current/average/max heart rate, HR zone, GPS position, altitude, heading, ascent, calories and clock.

From the ESP32 board: ambient temperature and humidity. The current dashboard shows temperature.

Cadence and cycling power are not claimed by v0.1. Their protocol bytes are reserved and sent as unknown because they require appropriate external sensor data.

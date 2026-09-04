# Getting started

This is the complete shortest path from source code to the first live activity. For an install-by-install explanation, including Java, Arduino, Garmin SDK Manager and why an MTP/“Android file transfer” utility may be needed on a Mac, use the [beginner-friendly guide](beginner-guide.md).

## Requirements

- macOS for the documented v0.1 workflow
- Python 3.11 or newer
- Arduino IDE or Arduino CLI with ESP32 core 3.3.11
- Arduino library U8g2 2.36.18
- Garmin Connect IQ SDK 9.2.0 and Java 17
- Waveshare ESP32-S3-RLCD-4.2 connected with a USB data cable
- a supported Fenix 7-family watch for live mode

## 1. Prepare the project

From the downloaded or cloned OpenRideMirror folder, run:

```sh
./orm setup
./orm doctor
./orm config validate
```

The root wrapper runs directly from source; no package installation or virtual environment is needed. Local configuration is written to `.orm/config.toml` and ignored by Git, and the defaults do not require editing. Resolve anything `doctor` marks as missing before continuing. `setup` prepares the Arduino dependencies but does not flash a device or install anything on the watch.

## 2. Verify the display without Garmin

```sh
./orm demo
```

Demo mode runs a repeating three-minute synthetic ride, including GPS movement and Push mode. Flashing writes to external hardware. If more than one serial device is present, pass the exact port:

```sh
./orm demo --port /dev/cu.usbmodemXXXX
```

## 3. Flash live mode to the display

```sh
./orm live
```

The display should advertise as `ORM`. It will show saved ride summaries while disconnected and wait for Garmin when no history exists.

Leave the ESP32 connected to USB for power, or power it from its battery after flashing. It does not need to remain connected to the Mac during a ride.

## 4. Build the Garmin data field

```sh
./orm garmin fenix7
```

The command prints the exact output path. For a regular Fenix 7 it is normally:

```text
.orm/build/garmin/OpenRideMirror-fenix7.prg
```

Use the device ID that matches your exact watch; see [Garmin setup](garmin.md) for the supported targets.

## 5. Copy the `.prg` to the watch

1. Close Garmin Express completely, or eject the watch from Garmin Express.
2. Connect the Garmin watch to the Mac with its USB cable.
3. Open the watch in Finder if it appears there. If it does not, use an MTP utility such as OpenMTP.
4. Open the watch's `GARMIN/APPS` directory.
5. Copy the matching `.prg` from `.orm/build/garmin/` into `GARMIN/APPS`.
6. Eject the watch cleanly, unplug it and allow it a moment to process the file.

The watch may move the `.prg` into hidden internal storage, so it can disappear from `GARMIN/APPS` after installation. That is normal. Installing another build with the same app ID can also update it silently without showing an update prompt.

OpenRideMirror is a **data field**, not an app launched from the watch's app list. The next step is what makes Garmin run it.

## 6. Add ORM Live to an activity

Do this separately for every activity type in which you want the display to work. For example, adding it to Bike does not automatically add it to Walk.

On a Fenix 7:

1. press **START** and highlight **Bike** or **Walk**;
2. hold **MENU** and open that activity's settings;
3. choose **Data Screens**;
4. edit an existing screen or add a new one;
5. select a field, then choose **Connect IQ Fields → ORM Live**;
6. save the data screen and return to the activity.

Menu labels can vary slightly between Garmin firmware versions. The important result is that **ORM Live is one of the data fields inside the activity profile**. You do not need to keep its page visible during the ride, but it must remain configured in that activity.

If ORM Live does not appear in the Connect IQ Fields list, restart the watch once and check again. If it is still absent, rebuild using the correct Garmin device ID and repeat the copy step.

## 7. Run the first live test

Use this order so it is obvious which part is waiting:

1. Power on the ESP32 with the live firmware installed. It should show its disconnected/waiting state.
2. Keep any computer BLE simulator switched off and power only one ORM display nearby.
3. On Garmin, press **START** and open the Bike or Walk activity that contains ORM Live.
4. If you want to watch connection progress, switch to the data page containing ORM Live. It should move through `SCAN`, `LINK` and then `LIVE`.
5. Wait outdoors for the Garmin GPS indicator to become ready.
6. Press **START** again to begin recording the activity.
7. Move for a short distance. Speed, distance, heart rate and time should update; the map appears after Garmin provides a valid GPS position.

The setup is working when the Garmin field says `LIVE`, the display leaves its waiting screen, and changing values appear on the display. GPS can take longer than the other metrics because it requires a valid outdoor position fix.

The connection is direct from Garmin to ESP32 over BLE. Do not pair it in macOS, Garmin Connect or the phone's Bluetooth settings, and do not enter a MAC address.

To finish the test, stop and save or discard the activity on Garmin in the usual way. The display should leave the live ride state after Garmin reports that the activity stopped.

For later rides the short routine is simply:

```text
Power on ORM display → open the configured Garmin activity → wait for GPS → start the activity
```

## 8. Generate a local map

The bundled firmware has a tiny synthetic sample. For a real area, open the hosted **[Create your map](https://mihaelmiklosic.github.io/OpenRideMirror/map-builder.html)** tool, choose the area and download its map pack. Then connect the display and run:

```sh
./orm map flash
```

For a fully local version of the map tool, run:

```sh
./orm map ui
```

Choose an area and build it, then rebuild live or demo firmware. The build command automatically overlays `.orm/generated/map/` onto a temporary staged sketch.

## Useful checks

```sh
./orm protocol check
./orm test
./orm release check
```

`orm release check` is intentionally strict about generated protocol files, private paths, compiled artifacts, tests and ESP partition usage. A Garmin compile and a physical ride test remain manual release gates.

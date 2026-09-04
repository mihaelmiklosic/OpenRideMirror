# Build and use OpenRideMirror

This is the canonical v0.1 guide from source code to a first live activity. It assumes macOS because that is the workflow currently tested and documented by the maintainer.

OpenRideMirror is a companion display, not a standalone bike computer. The Garmin watch records the activity and remains the source of GPS and ride telemetry. The ESP32 receives that data over Bluetooth Low Energy, draws the interface and offline map, reads its own ambient-temperature sensor, and stores a short local ride history.

## Before you begin

Allow about an hour for the first setup. You should be comfortable installing desktop applications, running commands in Terminal, connecting USB devices and copying a file to the watch.

### Hardware

- **Waveshare ESP32-S3-RLCD-4.2**, SKU **33298** with an included 18650 battery or SKU **33507 / ESP32-S3-RLCD-4.2-EN** without one;
- a USB data cable for the ESP32;
- a supported Garmin Fenix 7-family watch and its USB cable;
- a Mac for the documented v0.1 workflow.

The display must be the integrated 4.2-inch **400 × 300 reflective LCD** board using an ESP32-S3-WROOM-1-N16R8 module. OpenRideMirror rotates it into a 300 × 400 portrait canvas. A generic Waveshare 4.2-inch e-paper panel, ESP32 e-paper driver board or another ESP32 screen is not a drop-in replacement. Check [Reference hardware](docs/hardware.md) before buying.

### Software

- Python 3.11 or newer;
- Arduino IDE 2.x;
- Garmin Connect IQ SDK Manager and Connect IQ SDK 9.2.0;
- Java 17;
- an MTP file-transfer utility such as OpenMTP if the watch does not appear in Finder.

The setup helper installs the tested ESP32 Arduino core 3.3.11 and U8g2 2.36.18 through the Arduino CLI bundled with Arduino IDE.

### No Android phone or app is required

Garmin sends data directly from the watch to the display. A phone is not part of the live connection.

Some Garmin watches expose their USB storage through Media Transfer Protocol (MTP), a protocol also used by many Android devices. macOS may therefore need an MTP utility only to copy the locally built Garmin `.prg` file. This does not make Android part of OpenRideMirror.

## 1. Download the project

Clone the repository with Git, or download its source ZIP from GitHub and extract it. Open Terminal in the resulting `OpenRideMirror` folder. Every command in this guide is run from that root folder.

The repository intentionally contains no prebuilt ESP32 image or Garmin `.prg`. Each user builds both sides locally from source.

## 2. Prepare the ESP32 toolchain

Install [Arduino IDE 2.x](https://www.arduino.cc/en/software) as a normal Mac application and open it once. Then run:

```sh
./orm setup
```

This creates ignored local state under `.orm/` and installs the exact ESP32 core and U8g2 version used by the reference build. It needs internet access, but it does not flash the ESP32 or install anything on the watch.

Check the environment:

```sh
./orm doctor
```

At this stage the Arduino checks should report `OK`. Garmin-related checks can remain missing until you complete step 4.

You do not need to install the Python helper with `pip` or edit a TOML file. The root `./orm` wrapper runs it directly from this repository.

## 3. Verify the display without Garmin

Connect the ESP32 with a USB **data** cable and run:

```sh
./orm demo
```

This builds and flashes a self-running three-minute synthetic ride. The dashboard should update speed, heart rate and other statistics, move a fake GPS position across the sample map, and enter the Push view above 30 km/h. The loop proves that the board profile, display driver, map assets and rendering path work before Garmin or BLE is involved.

If the tool finds more than one possible serial device, pass the exact ESP32 port it reports:

```sh
./orm demo --port /dev/cu.usbmodemXXXX
```

Do not select a Bluetooth, headphone or debug-console serial port. If no `usbmodem` port appears, confirm that the cable supports data and reconnect the board.

## 4. Install Garmin's developer tools

Garmin tooling is installed separately because Garmin requires each developer to review and accept its SDK agreement personally. OpenRideMirror and its setup command cannot accept that agreement for you.

1. Install Java 17. With Homebrew, run `brew install openjdk@17`.
2. Download and open Garmin's [Connect IQ SDK Manager](https://developer.garmin.com/connect-iq/sdk/).
3. Review and accept Garmin's terms in SDK Manager.
4. Install Connect IQ SDK **9.2.0**.
5. Download the device package matching your exact Fenix model.
6. Make SDK 9.2.0 active, close SDK Manager and rerun `./orm doctor`.

The Garmin and Java checks should now report `OK`. See [Garmin reference](docs/garmin.md) for the complete list of compile-supported device IDs.

## 5. Flash live firmware to the display

Replace demo mode with the Garmin-connected receiver:

```sh
./orm live
```

The firmware is built in ignored local state and flashed to the connected board. The display advertises as `ORM` and waits for Garmin. USB is needed for the build and flash, but the computer is not needed during a ride; power the display by USB or a correctly installed board battery.

You can return to the synthetic test at any time with `./orm demo`.

## 6. Build the Garmin data field

For a regular Fenix 7, run:

```sh
./orm garmin fenix7
```

Use the product ID for your exact watch rather than assuming every Fenix variant shares the same binary. The command prints its output path, normally:

```text
.orm/build/garmin/OpenRideMirror-fenix7.prg
```

The first Garmin build also creates a private developer key under `.orm/keys/`. It is ignored by Git. Do not commit or share it; keep a private backup if you want to preserve the same local signing identity across rebuilds.

## 7. Copy the `.prg` to the watch

1. Close Garmin Express completely, or eject the watch from Garmin Express.
2. Connect the watch to the Mac by USB.
3. Open it in Finder if it appears there. Otherwise use an MTP utility such as [OpenMTP](https://openmtp.ganeshrvel.com/).
4. Open the watch's `GARMIN/APPS` directory.
5. Copy the `.prg` printed by the build command into `GARMIN/APPS`.
6. Eject the watch cleanly, unplug it and give it a moment to process the file.

The watch may move the `.prg` into internal storage, so the copied file can disappear from `GARMIN/APPS` after installation. That is normal. Installing another build with the same application ID can also update it without showing a separate update prompt.

OpenRideMirror is a Connect IQ **data field**, not an app launched from the watch's Apps list. Installing the file alone does not make it run.

## 8. Add ORM Live to Bike or Walk

Add the field separately to every activity profile where you want live data. On a Fenix 7:

1. press **START** and highlight **Bike**, **Walk** or another activity;
2. hold **MENU** and open that activity's settings;
3. choose **Data Screens**;
4. edit an existing screen or add a new one;
5. select a field, then choose **Connect IQ Fields → ORM Live**;
6. save the screen and return to the activity.

Menu wording can vary slightly with Garmin firmware. The important result is that **ORM Live is present on a data screen inside that activity profile**. It does not need to remain the visible page during the whole ride, but it must be configured so Garmin keeps the data field running.

Adding it to Bike does not automatically add it to Walk. Walking can provide GPS and telemetry when its own activity profile contains ORM Live.

## 9. Run the first live activity

Use this order for a clear first test:

1. Power one ORM display with live firmware installed.
2. Stop any computer BLE simulator and power off any second ORM display nearby.
3. Open the Bike or Walk activity containing ORM Live.
4. Switch to the ORM Live data page if you want to watch its status progress through `SCAN`, `LINK` and `LIVE`.
5. Go outside and wait for the Garmin GPS indicator to become ready.
6. Press **START** to begin recording, then move a short distance.

The connection is working when the Garmin field says `LIVE`, the display leaves its waiting screen and changing ride values appear. The map may appear later than heart rate or time because Garmin must first provide a current valid GPS location to the data field.

### BLE behavior

- Garmin is the BLE central/client; the ESP32 display is the peripheral/server.
- Discovery uses the exact advertised name `ORM` together with the ORM service UUID.
- No Bluetooth MAC address is configured or stored.
- Do not pair the display in macOS, Garmin Connect or the phone's Bluetooth settings.
- v0.1 uses an open connection with no persistent bond, authentication or encryption.
- A disconnect triggers another scan, so a replacement board can be found even when its BLE address changes.
- If two ORM displays are visible, the field reports `MULTIPLE ORM` instead of choosing one unpredictably.

The open link is convenient for development but telemetry is not private or authenticated. Read the [security model](SECURITY.md) before using the project.

For later rides, the normal routine is simply:

```text
Power on ORM display → open configured Garmin activity → wait for GPS → start
```

## 10. Add a map for your area

The firmware includes a small synthetic sample map so it can build without downloading private or location-specific data.

For the simplest custom-map workflow, open the hosted [OpenRideMirror map builder](https://mihaelmiklosic.github.io/OpenRideMirror/map-builder.html), choose a small area, select a detail level, inspect the monochrome preview and download the map pack. For a real map, the selected bounds are sent to a public Overpass instance to request OpenStreetMap data.

Connect the display and run:

```sh
./orm map flash
```

By default this finds the newest `OpenRideMirror-map-pack*.zip` in Downloads, validates and installs it, builds live firmware and flashes the board. Pass an explicit ZIP path if needed. To use the same map with the self-running demo instead:

```sh
./orm map flash --mode demo
```

For the local Python map UI, run `./orm map ui`. Advanced area configuration, offline cache behavior, emitted files, flash-size limits and OpenStreetMap licensing are documented in [Maps and attribution](docs/maps.md).

## Troubleshooting by layer

- **`./orm demo` fails:** solve the Arduino, USB, board or display problem before involving Garmin.
- **`./orm garmin` fails:** rerun `./orm doctor`, verify Java 17, the active Connect IQ SDK and the exact device package.
- **The watch is absent in Finder:** close Garmin Express and use an MTP utility.
- **The `.prg` disappears:** check whether ORM Live now appears under Connect IQ Fields; the watch may have processed and moved it.
- **Garmin remains on `SCAN` or `NOT FOUND`:** confirm live firmware is installed, the data field is in the active profile and only one ORM display is nearby.
- **Garmin says `LIVE`, but the display waits:** rebuild both sides from the same source and run `./orm protocol check`.
- **The map waits for GPS:** test outdoors with the activity recording and a valid Garmin GPS fix.

See [Troubleshooting](docs/troubleshooting.md) for detailed diagnoses and [Support](SUPPORT.md) for the information to include in an issue.

## Project status and independence

This is experimental open-source hardware/software, not a commercial or safety-certified bike computer. The development board and prototype mount are not waterproof or safety-rated.

OpenRideMirror is an independent, unofficial open-source project. It is not affiliated with, endorsed by or supported by Garmin. Garmin, Fenix and Connect IQ are trademarks of Garmin Ltd. or its subsidiaries.

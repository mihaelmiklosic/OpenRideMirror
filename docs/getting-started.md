# Getting started

This is the compact setup path for developers. For an install-by-install explanation, including Java, Arduino, Garmin SDK Manager, the `.prg` transfer and why an MTP/“Android file transfer” utility may be needed on a Mac, use the [beginner-friendly guide](beginner-guide.md).

## Requirements

- macOS for the documented v0.1 workflow
- Python 3.11 or newer
- Arduino IDE or Arduino CLI with ESP32 core 3.3.11
- Arduino library U8g2 2.36.18
- Garmin Connect IQ SDK 9.2.0 and Java 17
- Waveshare ESP32-S3-RLCD-4.2 connected with a USB data cable
- a supported Fenix 7-family watch for live mode

From the repository root, let ORM install the pinned Arduino packages and create its internal defaults:

```sh
./orm setup
./orm doctor
./orm config validate
```

The root wrapper runs directly from source; no package installation or virtual environment is needed. Local configuration is written to `.orm/config.toml` and ignored by Git. It is available for advanced settings, but the default build does not require editing it.

## 1. Verify the display without Garmin

```sh
./orm demo
```

Demo mode runs a repeating three-minute synthetic ride, including GPS movement and Push mode. Flashing writes to external hardware. If more than one serial device is present, pass the exact port:

```sh
./orm demo --port /dev/cu.usbmodemXXXX
```

## 2. Build live mode

```sh
./orm live
```

The display should advertise as `ORM`. It will show saved ride summaries while disconnected and wait for Garmin when no history exists.

## 3. Build the Garmin source

```sh
./orm garmin fenix7
```

The locally generated file is placed under `.orm/build/garmin/`. Follow [Garmin setup](garmin.md) to sideload it and add the data field to an activity profile.

## 4. Generate a local map

The bundled firmware has a tiny synthetic sample. For a real area:

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

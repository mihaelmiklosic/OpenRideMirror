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

Install the project CLI from the repository root:

```sh
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install -e tools
orm doctor
orm configure
orm config validate
```

Local configuration is written to `.orm/config.toml`; it is intentionally ignored by Git.

## 1. Verify the display without Garmin

```sh
orm build esp --mode demo
orm flash esp --mode demo
```

Demo mode runs a repeating three-minute synthetic ride, including GPS movement and Push mode. Flashing writes to external hardware. If more than one serial device is present, pass the exact port:

```sh
orm flash esp --mode demo --port /dev/cu.usbmodemXXXX
```

## 2. Build live mode

```sh
orm build esp --mode live
orm flash esp --mode live
```

The display should advertise as `ORM`. It will show saved ride summaries while disconnected and wait for Garmin when no history exists.

## 3. Build the Garmin source

```sh
orm build garmin --device fenix7
```

The locally generated file is placed under `.orm/build/garmin/`. Follow [Garmin setup](garmin.md) to sideload it and add the data field to an activity profile.

## 4. Generate a local map

The bundled firmware has a tiny synthetic sample. For a real area:

```sh
orm map ui
```

Choose an area and build it, then rebuild live or demo firmware. The build command automatically overlays `.orm/generated/map/` onto a temporary staged sketch.

## Useful checks

```sh
orm protocol check
orm test
orm release check
```

`orm release check` is intentionally strict about generated protocol files, private paths, compiled artifacts, tests and ESP partition usage. A Garmin compile and a physical ride test remain manual release gates.

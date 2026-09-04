# OpenRideMirror

OpenRideMirror (ORM) is an open-source cycling display that mirrors live activity data from a Garmin watch to a monochrome ESP32 screen over Bluetooth Low Energy.

Built by [Mihael Miklošić](https://mihaelmiklosic.com) · [@miha.experiments](https://www.instagram.com/miha.experiments/)

The watch remains the GPS and activity sensor hub. The ESP32 is a lightweight external display: it receives speed, distance, heart rate, HR zone, position, elevation, ascent, calories and ride time, then combines them with ambient temperature measured on the display board. It also keeps a small ride history locally.

The source also includes a [browser recreation](web-demo/index.html) with sample ride data and a GitHub Pages map builder that downloads ready-to-compile map `.h` files as one ZIP. Beginners do not need to edit the internal TOML configuration.

> Project status: **v0.1 developer release**. The Garmin Fenix 7 and Waveshare ESP32-S3-RLCD-4.2 combination has been tested on physical hardware. Other listed Fenix 7-family targets compile but still need community hardware testing.

## How it works

```text
Garmin activity + ORM data field
        │  BLE GATT, ORM protocol v1
        ▼
ESP32-S3 receiver ── local SHTC3 temperature
        │
        ▼
400 × 300 reflective LCD dashboard + local ride history
```

The ESP32 advertises the exact name `ORM` and a project-specific service UUID. The Garmin data field scans for both, connects to exactly one matching display, and streams three fixed-size packets in rotation. No phone, internet connection or BLE MAC configuration is required.

## Start here

1. If you want the least technical route, read the [beginner-friendly macOS guide](docs/beginner-guide.md). The shorter developer version is [Getting started](docs/getting-started.md).
2. Install Arduino IDE 2.x as a normal Mac application.
3. Run `./orm setup` from this folder. It creates the internal defaults and installs the exact ESP32 core and U8g2 library automatically.
4. Connect the display and run `./orm demo`. It builds and flashes the self-running test in one step.
5. Install Garmin's Connect IQ tools and build the live firmware and data field following [Garmin setup](docs/garmin.md).

You do not need `pip`, a Python environment or a TOML file. The root-level `./orm` command runs the helper directly from this repository.

The short everyday commands are `./orm demo`, `./orm live`, `./orm garmin` and `./orm map flash`. The longer `build` and `flash` subcommands remain available for development and debugging.

The repository intentionally contains **source only**. Compiled ESP images, Garmin `.prg` files, signing keys and personal activity files are ignored. Each user builds locally.

No Android phone or companion phone app is required. On macOS, an MTP file-transfer utility may be needed only to copy the locally built `.prg` onto a Fenix watch that does not appear in Finder.

## Repository map

| Path | Purpose |
|---|---|
| `firmware/esp32/OpenRideMirror` | ESP32 receiver, display UI, sensor and sample map |
| `garmin/OpenRideMirror` | Connect IQ data field that publishes activity data |
| `protocol` | Machine-readable ORM v1 schema, generated constants and golden packets |
| `tools` | The `orm` build, map, simulation and release CLI |
| `simulator` | Canonical three-minute sample route and BLE simulator |
| `web-demo` | Browser interface recreation and client-side map pack generator |
| `examples` | Shareable configurations and synthetic map input |
| `docs` | Hardware, architecture, maps, porting and troubleshooting |

## Supported hardware

The reference target is the **Waveshare ESP32-S3-RLCD-4.2** development board—specifically Waveshare SKU **33298** (battery included) or **33507 / ESP32-S3-RLCD-4.2-EN** (battery not included). It uses an ESP32-S3-WROOM-1-N16R8 module, 16 MB flash, 8 MB PSRAM and an integrated 4.2-inch 300 × 400 monochrome reflective LCD. It is an **RLCD, not an e-paper display**. The board also provides the SHTC3 sensor used for ambient temperature. See the [exact Waveshare product](https://www.waveshare.com/product/esp32-s3-rlcd-4.2.htm) and [Hardware](docs/hardware.md).

Do not accidentally buy a generic 4.2-inch Waveshare e-paper panel, an ESP32 e-paper driver board, or another similarly named ESP32-S3 LCD board: the included driver, pins and build profile target this exact integrated RLCD board. Porting is possible, but another display needs its own driver and rendering adapter; “any ESP32 with a screen” is not automatically compatible. See [Porting](docs/porting.md).

The Garmin manifest currently includes the Fenix 7, 7S, 7X and Pro variants listed in [Garmin setup](docs/garmin.md). Only the Fenix 7 has been physically verified by the maintainer.

## Maps

ORM uses a compact, offline, compile-time map. The included firmware map is synthetic and safe to redistribute. The simplest route is the GitHub Pages map builder: download its ZIP, connect the display and run `./orm map flash`. Developers can also use `./orm map ui` or configure a bounding box, center/radius or GPX buffer. Generated data stays in `.orm/` and is not committed by default. The browser demo contains a separately attributed, clipped OSM-derived extract. See [Maps and attribution](docs/maps.md).

## Security model

Version 0.1 deliberately uses an open, unencrypted BLE GATT link with no bonding. This makes replacement boards and local development simple, but nearby devices can observe the advertised service and may attempt to connect or write. Do not treat telemetry as private or authenticated. See [Security](SECURITY.md).

## AI disclosure

AI-assisted development was used extensively for implementation, debugging, documentation and tooling, with the maintainer directing the design and validating builds and hardware behavior. Details are in [AI_USAGE.md](AI_USAGE.md).

## License

Project code is licensed under [GPL-3.0-only](LICENSE). Bundled third-party assets retain their own licenses; see [ATTRIBUTION.md](ATTRIBUTION.md).

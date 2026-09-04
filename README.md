# OpenRideMirror

OpenRideMirror (ORM) is an open-source cycling display that mirrors live activity data from a Garmin watch to a monochrome ESP32 screen over Bluetooth Low Energy.

The watch remains the GPS and activity sensor hub. The ESP32 is a lightweight external display: it receives speed, distance, heart rate, HR zone, position, elevation, ascent, calories and ride time, then combines them with ambient temperature measured on the display board. It also keeps a small ride history locally.

The source also includes a deterministic [browser recreation](web-demo/index.html) of the 300 × 400 interface for design review and GitHub Pages.

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
2. Install the local tool with `python3 -m pip install -e tools`.
3. Run `orm doctor`, then `orm configure`.
4. Build a demo firmware with `orm build esp --mode demo` before trying Garmin BLE.
5. Build the live firmware and Garmin data field following [Garmin setup](docs/garmin.md).

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
| `web-demo` | Browser recreation of the 300 × 400 display interface |
| `examples` | Shareable configurations and synthetic map input |
| `docs` | Hardware, architecture, maps, porting and troubleshooting |

## Supported hardware

The reference target is the **Waveshare ESP32-S3-RLCD-4.2** with its 400 × 300 ST7305 monochrome reflective LCD and onboard SHTC3 sensor. Porting to another ESP32 is possible, but another display needs its own driver and rendering adapter; “any ESP32 with a screen” is not automatically compatible. See [Porting](docs/porting.md).

The Garmin manifest currently includes the Fenix 7, 7S, 7X and Pro variants listed in [Garmin setup](docs/garmin.md). Only the Fenix 7 has been physically verified by the maintainer.

## Maps

ORM uses a compact, offline, compile-time map. The included firmware map is synthetic and safe to redistribute. To generate your own map from OpenStreetMap, use `orm map ui` or configure a bounding box, center/radius or GPX buffer and run `orm map build`. Generated data stays in `.orm/` and is not committed by default. The browser demo contains a separately attributed, clipped OSM-derived extract. See [Maps and attribution](docs/maps.md).

## Security model

Version 0.1 deliberately uses an open, unencrypted BLE GATT link with no bonding. This makes replacement boards and local development simple, but nearby devices can observe the advertised service and may attempt to connect or write. Do not treat telemetry as private or authenticated. See [Security](SECURITY.md).

## AI disclosure

AI-assisted development was used extensively for implementation, debugging, documentation and tooling, with the maintainer directing the design and validating builds and hardware behavior. Details are in [AI_USAGE.md](AI_USAGE.md).

## License

Project code is licensed under [GPL-3.0-only](LICENSE). Bundled third-party assets retain their own licenses; see [ATTRIBUTION.md](ATTRIBUTION.md).

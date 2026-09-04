# Instructions for AI coding agents

These instructions apply to the entire OpenRideMirror repository.

## Product boundaries

- `ORM` is the stable BLE identity and project acronym.
- Protocol v1 source of truth is `development/protocol/orm-protocol.json`.
- The ESP32 is the BLE peripheral; Garmin and the desktop simulator are centrals.
- Discovery matches exact name `ORM` and the service UUID. Do not add a hardcoded BLE MAC or persistent bonding without an explicit versioned design decision.
- Temperature/humidity come from the ESP32 board. Garmin supplies activity and GPS data.
- Cadence and power are reserved/unknown in v0.1. Do not display them as available measurements.
- Audio is out of scope for v0.1.

## Preserve source-only releases

- Never commit `.prg`, `.bin`, `.elf`, signing keys, build directories, personal FIT/GPX data or downloaded OpenStreetMap caches.
- Checked-in firmware map assets must remain the redistributable synthetic sample. The separately attributed browser-demo OSM extract may be updated only with attribution and ODbL review.
- Generated maps and all local builds belong under `.orm/`.
- Do not add credentials or absolute developer-machine paths.

## Compatibility and testing

- A protocol change must update schema, generators, golden fixtures, Python, Monkey C, ESP parsing and docs together.
- Reject malformed packets; never trust BLE payloads.
- Run `orm protocol check`, `orm test`, and both ESP builds before claiming completion.
- Compile all Garmin targets after Monkey C changes. Label targets as hardware-tested or compile-only accurately.
- Do not claim frame rate, latency, battery life or compatibility without measurement.

## Hardware target

- Reference device: Waveshare ESP32-S3-RLCD-4.2, 400 × 300 ST7305 monochrome reflective LCD.
- Logical UI: 300 × 400 portrait.
- Preserve the exact FQBN documented in `docs/hardware.md` unless adding a separate named target.
- Keep the known-good custom ST7305 driver independently reviewable.

## Editing

- Keep changes focused and preserve user work.
- Prefer generated constants over copied identifiers.
- Update documentation when commands, requirements or supported targets change.
- Disclose AI-assisted work honestly and validate it like any other contribution.

## Repository layout

- `firmware/` and `garmin/` are the two product source trees users need to build.
- `docs/` contains optional guides and reference material; keep the root README and `GETTING_STARTED.md` as the shortest public path.
- `web-demo/` contains only the published interface demo and map builder.
- `development/` contains the CLI implementation, protocol schema, tests, simulator fixtures, examples and maintenance scripts. These support the project but are not required reading for a first build.

## Beginner workflow

- Prefer root-level `./orm` commands in user-facing instructions. The wrapper runs directly from source; do not ask beginners to install the package, activate a venv or edit TOML.
- `./orm setup` installs the pinned ESP32 Arduino core and U8g2 once Arduino IDE is present.
- Prefer the one-step `./orm demo`, `./orm live` and `./orm garmin DEVICE` commands for beginner instructions. Keep separate build/flash commands for debugging.
- For a browser-generated map pack, the shortest complete hardware path is `./orm map flash`; it finds the newest matching ZIP in Downloads. Explain explicit paths or separate install/build/flash commands only when the user needs developer control.
- Garmin Connect IQ SDK installation and agreement acceptance remain manual user steps.

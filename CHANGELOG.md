# Changelog

All notable project changes will be documented here.

## [Unreleased]

### Added

- Standalone OpenRideMirror ESP32 firmware with live and three-minute demo modes.
- Garmin Connect IQ data field for Fenix 7-family targets.
- ORM BLE protocol v1 with schema, generated constants and golden packet fixtures.
- Open BLE discovery by exact name and service UUID, without MAC configuration or saved bonding.
- Reflective LCD dashboard, Push view, GPS map, SHTC3 temperature and checksummed ride history.
- Python `orm` CLI for environment checks, builds, flashing, maps, simulation and release checks.
- Offline map compiler with synthetic sample and optional OpenStreetMap/Overpass input.
- Browser interface recreation, documentation, CI and contribution guidance.
- Beginner-facing identification of the exact compatible Waveshare board and its battery/no-battery SKUs.
- Browser-based OpenStreetMap-to-firmware map pack generator and one-command map install/build/flash flow.
- Root-level, no-install `./orm` wrapper plus one-step setup, demo, live and Garmin commands.
- Creator links for Mihael Miklošić and miha.experiments.
- Public data-flow and offline-map diagrams plus a project FAQ based on prototype questions.
- Project gallery with frames from the original road-test Reel, dashboard demo and browser map builder.
- End-to-end first-use instructions covering Garmin sideloading, activity data-field setup and the first live connection.
- Separate interface-demo controls from the standalone map-builder workflow.

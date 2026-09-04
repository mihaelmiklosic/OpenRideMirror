# Development files

You do not need to read or edit this directory to build OpenRideMirror. Use the commands in the root [build guide](../GETTING_STARTED.md).

This directory keeps maintenance infrastructure out of the main project path:

- `tools/` implements the root `./orm` helper;
- `protocol/` documents and checks the BLE packet format shared by Garmin and ESP32;
- `tests/` verifies the helper, configuration, map builder and protocol;
- `examples/` contains synthetic map and configuration input;
- `simulator/` contains the shareable sample route;
- `scripts/` contains compatibility wrappers for older development workflows.

The protocol files are not a separate installation step. They exist so changes on the Garmin and ESP32 sides cannot silently disagree about packet fields, UUIDs or versions.

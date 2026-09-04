# ORM protocol v1

The canonical definition is `protocol/orm-protocol.json`. It defines transport identifiers, sentinels, enums and byte offsets. Generated constants and golden fixtures make accidental incompatibility visible.

## Transport

- BLE peripheral name: `ORM`
- service UUID: `D8185099-1302-4FEB-906F-0AE8D5329ABA`
- telemetry characteristic UUID: `734A1ED9-8E4D-4AEB-A5D7-BEABC20643B8`
- packet size: 20 bytes
- byte order: little-endian
- Garmin write type: with response

Every packet begins with type, protocol version and an 8-bit sequence number. Version 1 defines:

| Type | Name | Purpose |
|---:|---|---|
| `0x10` | activity | timer, sport, HR, time, distance, current speed |
| `0x11` | GPS | quality, position, altitude, heading, activity timestamp |
| `0x12` | extended | HR zone/averages, speed averages, ascent, kcal, clock |

Unsigned unknown values use all-one sentinels (`0xff` or `0xffff`); signed 16-bit unknown uses `-32768`. The reserved positions in the activity packet remain unknown in v0.1 and must not be interpreted as real measurements.

## Generated files

```sh
orm protocol generate
orm protocol check
```

Generation creates C++, Monkey C and Python constants under `protocol/generated/`. Consumers still contain validation and packet-specific domain logic; the release check verifies their UUID/version markers against the schema.

## Compatibility rules

- Never change the meaning or size of a released packet in place.
- Add fields only through a new packet type or a new protocol version.
- Receivers must reject unknown versions and malformed values.
- Senders should keep only current telemetry; stale queues are worse than dropped samples.
- A release that changes protocol identifiers must update schema, generated files, fixtures, Garmin source, firmware and documentation together.

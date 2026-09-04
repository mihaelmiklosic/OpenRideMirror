# BLE discovery and connection

The word “pairing” is easy to misunderstand here. OpenRideMirror v0.1 does not configure a Fenix MAC address and does not use an authenticated saved bond.

1. The ESP32 advertises the exact local name `ORM` and ORM service UUID.
2. The Garmin data field scans for two seconds.
3. A result must match both the exact name and service UUID.
4. Exactly one match is connected through the Connect IQ BLE API.
5. If two ORM receivers are visible, the field reports `MULTIPLE ORM` instead of choosing unpredictably.
6. Disconnects trigger a new scan, so a replacement ESP32 with a different BLE MAC is found automatically.

The service UUID prevents an unrelated device named ORM from matching; the name makes the device recognizable. The BLE address is intentionally not part of project configuration.

## Expected display-field states

| Status | Meaning |
|---|---|
| `SCAN` | scanning for an exact ORM advertisement |
| `NOT FOUND` | no match; retry follows |
| `LINK` | connection or service discovery in progress |
| `LIVE` | telemetry writes are succeeding |
| `MULTIPLE ORM` | more than one distinct receiver matched |
| `PAIR ERR` | Connect IQ could not establish the connection |
| `NO SERVICE` / `NO DATA` | advertisement matched but expected GATT objects did not |
| `WRITE ERR` | a telemetry write failed |

For security and privacy consequences of open BLE, read [Security](../SECURITY.md).

# Security policy

## Supported version

Only the latest source on the default branch is supported during the v0.x development phase.

## BLE privacy model

ORM v0.1 advertises a recognizable service and accepts an unencrypted, unauthenticated GATT connection. It intentionally does not use BLE bonding, application-level authentication or payload encryption.

Consequences:

- nearby devices can detect that an ORM display is present;
- telemetry is not confidential against a capable nearby observer;
- a nearby client may attempt to connect or send crafted packets;
- service UUID and device name matching provide discovery filtering, not security.

The receiver validates packet shape and plausible ranges, but this is not authentication. Do not use ORM for safety-critical navigation, medical decisions or private telemetry. Do not expose control of the bike or other dangerous actuators through this protocol.

## Reporting a vulnerability

Avoid publishing a working exploit before the maintainer can assess it. Open a minimal GitHub security advisory if the repository enables private reporting; otherwise contact the maintainer through the profile associated with the repository. Include affected commit/version, reproduction conditions and realistic impact. Never include someone else’s location history or private activity data.

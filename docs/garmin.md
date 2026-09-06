# Garmin reference

For the complete installation and first-use sequence, follow [Build and use OpenRideMirror](../GETTING_STARTED.md). This page is the device and behavior reference.

OpenRideMirror is a Connect IQ **data field**, not a standalone watch app. Garmin runs it only while it is included on a data screen of the active activity profile.

## Current targets

| Connect IQ device ID | Model group | Status |
|---|---|---|
| `fenix7` | Fenix 7 | Physically tested |
| `fenix7s` | Fenix 7S | Compile-tested |
| `fenix7x` | Fenix 7X | Compile-tested |
| `fenix7pro` | Fenix 7 Pro | Compile-tested |
| `fenix7pronowifi` | Fenix 7 Pro no-Wi-Fi variant | Compile-tested |
| `fenix7spro` | Fenix 7S Pro | Compile-tested |
| `fenix7xpro` | Fenix 7X Pro | Compile-tested |
| `fenix7xpronowifi` | Fenix 7X Pro no-Wi-Fi variant | Compile-tested |
| `fenix843mm` | Fenix 8 43mm AMOLED | Compile-tested |

Compile success does not establish real-device compatibility. A new watch family needs its exact manifest product ID plus hardware verification of BLE behavior, update cadence and available activity fields.

## Build reference

Build one target:

```sh
./orm garmin fenix7
```

Build every listed target:

```sh
./orm build garmin --all
```

The CLI uses the active Connect IQ SDK, creates a private developer key under `.orm/keys/` when needed, and writes `.prg` files to `.orm/build/garmin/`. Keys and build output are ignored by Git.

Garmin requires each user to review and accept the Connect IQ SDK agreement personally. OpenRideMirror does not bundle the SDK or accept its terms automatically.

The canonical guide explains how to copy the resulting `.prg` through Finder or MTP, add **ORM Live** to Bike or Walk, and run the first activity. The watch may move an installed `.prg` into internal storage or update the same app ID without showing a separate prompt.

## Unit tests

```sh
./orm test --garmin --device fenix843mm
```

The Connect IQ simulator does not emulate BLE. It accepts `registerProfile()`
but never delivers `onProfileRegister()`, so a data field launched there stays
on `INIT` and the discovery state machine is never exercised — running the app
in the simulator proves it starts and draws, nothing more.

The Monkey C tests in `garmin/OpenRideMirror/test/` drive the delegate callbacks
directly, which covers the `INIT` → `SCAN` → `NOT FOUND` → retry transitions
with no watch and no ESP32 board. They deliberately avoid `start()`, because
that registers a real BLE profile against simulator-global state and repeated
registrations fail with `Too Many Profiles`.

The simulator must already be running; `monkeydo` talks to it over a local
socket. This is why the Garmin tests are opt-in rather than part of `orm test`.

Passing these tests is not evidence of hardware compatibility. They pin the
logic, not the radio.

## Activity lifecycle

- ORM Live must be added separately to every activity profile that should stream data.
- Installing the `.prg` does not make the field run globally.
- The field can remain on a non-visible data page, but it must stay configured in the active profile.
- Opening that activity starts BLE discovery; starting the timer supplies recorded activity values.
- Walking can provide GPS as well as cycling when Garmin exposes a valid current location.

Typical field states are documented in [BLE discovery and connection](pairing.md); failures are covered in [Troubleshooting](troubleshooting.md).

## Data supplied by Garmin

ORM protocol v1 can carry:

- activity and timer state;
- sport type;
- current and average speed;
- distance and elapsed activity time;
- current, average and maximum heart rate plus HR zone;
- GPS position, quality, altitude and heading;
- total ascent and calories;
- the watch's local time.

Ambient temperature and humidity come from the ESP32 board's SHTC3 sensor, not the watch. The current dashboard shows temperature.

Coordinates are scaled to 1e7 with rounding rather than truncation. Truncating
biased every fix roughly a centimetre toward the equator and the prime meridian,
and stopped the Monkey C encoder from reproducing the shared golden fixtures
exactly. The wire format is unchanged; only the last unit of the scaled integer
moves.

Cadence and cycling power are not claimed by v0.1. Their protocol positions remain reserved and unknown because no suitable data source is integrated.

## Independence

OpenRideMirror is an independent, unofficial open-source project. It is not affiliated with, endorsed by or supported by Garmin. Garmin, Fenix and Connect IQ are trademarks of Garmin Ltd. or its subsidiaries.

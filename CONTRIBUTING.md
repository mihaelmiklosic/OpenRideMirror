# Contributing

Bug reports, hardware test results, documentation fixes and focused ports are welcome.

## Before changing code

- Read `AGENTS.md` and the architecture/protocol documentation.
- Open an issue before a breaking protocol change or a large new hardware abstraction.
- Never commit developer keys, compiled Garmin/ESP artifacts, serial-port paths, personal activities, GPS history, Wi-Fi credentials or downloaded map caches.
- Preserve the checked-in synthetic map. Generate personal maps under `.orm/`.

## Local checks

```sh
python3 -m pip install -e tools
orm doctor
orm protocol check
orm test
orm build esp --mode demo
orm build esp --mode live
```

Run `orm build garmin --all` when changing Monkey C or the protocol. For UI/hardware work, describe what was tested on the physical display. A compile-only target must be labeled compile-only.

## Pull requests

Keep changes scoped and explain:

- the problem and chosen behavior;
- hardware/watch combinations tested;
- commands run;
- protocol or map-format effects;
- whether AI assistance was used and what you verified.

All contributed project code is accepted under GPL-3.0-only. Do not add an asset unless its redistribution terms are clear and its license/attribution is included.

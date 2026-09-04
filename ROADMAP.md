# Roadmap

## v0.1: source release

- reference ESP32 receiver and reflective LCD UI
- Garmin Fenix 7-family data-field builds
- ORM protocol v1 and golden fixtures
- synthetic demo ride and browser recreation
- local map generator with OpenStreetMap attribution
- source-only release and repeatable checks

## Likely next steps

- validate more Fenix 7-family devices on real hardware
- extract a clearer display-driver/rendering boundary for additional screens
- improve map label placement and memory estimates
- add optional protocol authentication only through a versioned, interoperable design
- design and publish a proper enclosure and bike mounting system
- document measured latency and power behavior with reproducible methodology

Audio prompts, cadence and cycling power are not part of v0.1. They should not be added to the UI until their data sources and hardware requirements are explicit.

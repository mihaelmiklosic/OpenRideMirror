# Maps and attribution

OpenRideMirror does not download map tiles while riding. A development tool converts a small geographic area into compact C++ headers that are compiled into the ESP32 firmware.

## Fast path: local map UI

```sh
orm map ui
```

The UI binds only to `127.0.0.1`, uses a random session token for writes, and can build one of four area types:

- bundled synthetic sample;
- bounding box: south, west, north, east;
- center point plus radius;
- GPX track plus a surrounding buffer.

Output is written to `.orm/generated/map/`. Run `orm build esp` afterward; the build stages the firmware and overlays those headers without changing checked-in sample files.

## Configuration examples

Center/radius:

```toml
[map]
area_type = "center-radius"
center = [45.793, 15.958]
radius_km = 7.0
preset = "balanced"
labels = true
green_texture = true
overpass_endpoint = "https://overpass-api.de/api/interpreter"
```

Bounding box uses `bbox = [south, west, north, east]`. GPX buffer uses `gpx = "/path/to/route.gpx"` and `buffer_km = 2.0`. Keep personal GPX files outside the repository or under ignored local state.

Build without the browser:

```sh
orm map build
```

`--offline` requires a matching response already present in `.orm/cache/overpass/`; it never silently substitutes unrelated data.

## Presets

- `minimal`: major roads, secondary roads, streets and cycleways.
- `balanced`: also includes paths and tracks.
- `detailed`: additionally includes selected service roads.

More detail costs flash space and can reduce legibility on a 300 × 400 monochrome display. The generator clips and quantizes roads into tiles, ranks labels, converts names to the limited display character set, and creates a sparse green-area texture mask.

## What gets emitted

- `OrmMapData.h`: geographic bounds and quantized road segments;
- `OrmMapLabels.h`: label anchors, text and priority;
- `OrmGreenMask.h`: coarse vegetation/park mask;
- `map-preview.json`: browser-friendly preview data;
- `map-manifest.json`: source hash, bounds, preset and output counts.

`orm release check` builds both firmware modes and enforces a partition headroom threshold. Always inspect the physical screen too; a successful compile does not guarantee a readable map.

## OpenStreetMap attribution

The checked-in firmware Jarun sample is synthetic and marked `SAMPLE`. The browser demo contains a small, clipped OSM-derived extract and visibly renders `© OSM`. When you generate from OpenStreetMap/Overpass, the rendered map must also visibly credit OpenStreetMap contributors. The generated firmware attribution string is `© OSM`.

OpenStreetMap data is licensed under ODbL 1.0. If you publish or distribute generated map databases or firmware containing them, review the [OpenStreetMap copyright and licence guidance](https://www.openstreetmap.org/copyright) and your obligations under ODbL. A copy of ODbL is included in `third_party/licenses/ODbL-1.0.txt`. This project documentation is not legal advice.

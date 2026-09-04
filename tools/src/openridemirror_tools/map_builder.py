from __future__ import annotations

import hashlib
import json
import math
import os
import unicodedata
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

from .paths import repo_root, state_dir

MAJOR = {"motorway", "motorway_link", "trunk", "trunk_link", "primary", "primary_link"}
SECONDARY = {"secondary", "secondary_link", "tertiary", "tertiary_link"}
STREET = {"residential", "unclassified", "living_street", "road"}
BIKE = {"cycleway"}
PATH = {"track", "path", "footway", "pedestrian", "bridleway", "steps"}
GREEN_TAGS = {"forest", "grass", "meadow", "recreation_ground", "village_green"}


@dataclass
class Road:
    style: int
    points: list[tuple[float, float]]
    name: str = ""


@dataclass
class Label:
    longitude: float
    latitude: float
    name: str
    rank: int
    place: bool = False


def ascii_name(value: str) -> str:
    value = value.translate(str.maketrans({"Đ": "D", "đ": "d", "Ł": "L", "ł": "l"}))
    plain = unicodedata.normalize("NFKD", value)
    plain = "".join(character for character in plain if not unicodedata.combining(character))
    return " ".join(plain.replace('"', "").split())[:22]


def bounds_from_config(config: dict[str, Any]) -> tuple[float, float, float, float]:
    map_config = config.get("map", {})
    kind = map_config.get("area_type", "sample")
    if kind == "sample":
        return 45.75, 15.90, 45.82, 16.00
    if kind == "bbox":
        south, west, north, east = map(float, map_config["bbox"])
    elif kind == "center-radius":
        latitude, longitude = map(float, map_config["center"])
        radius = float(map_config["radius_km"])
        south = latitude - radius / 110.54
        north = latitude + radius / 110.54
        west = longitude - radius / (111.32 * math.cos(math.radians(latitude)))
        east = longitude + radius / (111.32 * math.cos(math.radians(latitude)))
    elif kind == "gpx-buffer":
        points = gpx_points(Path(map_config["gpx"]).expanduser())
        if not points:
            raise ValueError("GPX file contains no track points")
        buffer_km = float(map_config["buffer_km"])
        latitudes = [point[1] for point in points]
        longitudes = [point[0] for point in points]
        middle = sum(latitudes) / len(latitudes)
        south = min(latitudes) - buffer_km / 110.54
        north = max(latitudes) + buffer_km / 110.54
        west = min(longitudes) - buffer_km / (111.32 * math.cos(math.radians(middle)))
        east = max(longitudes) + buffer_km / (111.32 * math.cos(math.radians(middle)))
    else:
        raise ValueError(f"unsupported map area type: {kind}")
    if not (-90 <= south < north <= 90 and -180 <= west < east <= 180):
        raise ValueError("invalid map bounds")
    return south, west, north, east


def gpx_points(path: Path) -> list[tuple[float, float]]:
    root = ET.parse(path).getroot()
    points: list[tuple[float, float]] = []
    for element in root.iter():
        if element.tag.rsplit("}", 1)[-1] in {"trkpt", "rtept", "wpt"}:
            points.append((float(element.attrib["lon"]), float(element.attrib["lat"])))
    return points


def style_for(tags: dict[str, Any], preset: str) -> int | None:
    highway = tags.get("highway")
    if highway in MAJOR:
        return 0
    if highway in SECONDARY:
        return 1
    if highway in STREET:
        return 2
    if highway in BIKE:
        return 3
    if preset != "minimal" and highway in PATH:
        return 4
    if preset == "detailed" and highway == "service" and tags.get("service") != "parking_aisle":
        return 5
    if tags.get("waterway"):
        return 6
    return None


def load_sample() -> tuple[list[Road], list[Label], list[list[tuple[float, float]]], dict[str, Any]]:
    path = repo_root() / "examples" / "maps" / "jarun" / "source.json"
    source = json.loads(path.read_text())
    roads = [Road(int(item["style"]), [tuple(point) for point in item["points"]], item.get("name", ""))
             for item in source["roads"]]
    labels = []
    for road in roads:
        if road.name:
            point = road.points[len(road.points) // 2]
            labels.append(Label(point[0], point[1], road.name, min(5, road.style)))
    labels.extend(Label(item["point"][0], item["point"][1], item["name"], item.get("rank", 3), True)
                  for item in source.get("places", []))
    green = [[tuple(point) for point in ring] for ring in source.get("green", [])]
    return roads, labels, green, {"kind": "synthetic", "path": str(path.relative_to(repo_root()))}


def overpass_query(bounds: tuple[float, float, float, float]) -> str:
    south, west, north, east = bounds
    box = f"{south:.7f},{west:.7f},{north:.7f},{east:.7f}"
    return f"""[out:json][timeout:90];
(
  way[\"highway\"]({box});
  way[\"waterway\"]({box});
  way[\"leisure\"=\"park\"]({box});
  way[\"landuse\"~\"forest|grass|meadow|recreation_ground|village_green\"]({box});
  way[\"natural\"=\"wood\"]({box});
  node[\"place\"]({box});
);
out tags center geom;"""


def fetch_overpass(bounds: tuple[float, float, float, float], offline: bool,
                   endpoint: str) -> tuple[dict[str, Any], dict[str, Any]]:
    query = overpass_query(bounds)
    digest = hashlib.sha256(query.encode()).hexdigest()
    cache = state_dir() / "cache" / "overpass" / f"{digest}.json"
    if cache.exists():
        payload = cache.read_bytes()
        return json.loads(payload), {"kind": "overpass-cache", "sha256": hashlib.sha256(payload).hexdigest()}
    if offline:
        raise FileNotFoundError("no cached Overpass response for this area")
    request = urllib.request.Request(
        endpoint,
        data=urllib.parse.urlencode({"data": query}).encode(),
        headers={"User-Agent": "OpenRideMirror/0.1 (+https://github.com/)"},
        method="POST",
    )
    with urllib.request.urlopen(request, timeout=120) as response:
        payload = response.read()
    cache.parent.mkdir(parents=True, exist_ok=True)
    cache.write_bytes(payload)
    return json.loads(payload), {"kind": "overpass", "endpoint": endpoint,
                                 "sha256": hashlib.sha256(payload).hexdigest()}


def parse_overpass(source: dict[str, Any], preset: str) -> tuple[list[Road], list[Label], list[list[tuple[float, float]]]]:
    roads: list[Road] = []
    labels: list[Label] = []
    green: list[list[tuple[float, float]]] = []
    for element in source.get("elements", []):
        tags = element.get("tags") or {}
        geometry = [(float(point["lon"]), float(point["lat"]))
                    for point in element.get("geometry", []) if "lon" in point and "lat" in point]
        style = style_for(tags, preset)
        name = ascii_name(tags.get("name", ""))
        if style is not None and len(geometry) >= 2:
            roads.append(Road(style, geometry, name))
            if name:
                anchor = geometry[len(geometry) // 2]
                labels.append(Label(anchor[0], anchor[1], name, min(style, 5)))
        is_green = tags.get("leisure") == "park" or tags.get("landuse") in GREEN_TAGS or tags.get("natural") == "wood"
        if is_green and len(geometry) >= 4:
            if geometry[0] != geometry[-1]:
                geometry.append(geometry[0])
            green.append(geometry)
        if tags.get("place") and name:
            center = element.get("center") or element
            if "lon" in center and "lat" in center:
                rank = {"city": 0, "town": 1, "suburb": 2, "village": 3}.get(tags["place"], 4)
                labels.append(Label(float(center["lon"]), float(center["lat"]), name, rank, True))
    return roads, labels, green


def clip_segment(x0: float, y0: float, x1: float, y1: float, left: float,
                 bottom: float, right: float, top: float) -> tuple[float, float, float, float] | None:
    dx, dy = x1 - x0, y1 - y0
    p, q = (-dx, dx, -dy, dy), (x0 - left, right - x0, y0 - bottom, top - y0)
    u0, u1 = 0.0, 1.0
    for pi, qi in zip(p, q):
        if pi == 0:
            if qi < 0:
                return None
            continue
        ratio = qi / pi
        if pi < 0:
            if ratio > u1:
                return None
            u0 = max(u0, ratio)
        else:
            if ratio < u0:
                return None
            u1 = min(u1, ratio)
    return x0 + u0 * dx, y0 + u0 * dy, x0 + u1 * dx, y0 + u1 * dy


def contains(ring: list[tuple[float, float]], longitude: float, latitude: float) -> bool:
    inside = False
    previous = ring[-1]
    for current in ring:
        x0, y0 = previous
        x1, y1 = current
        if (y0 > latitude) != (y1 > latitude):
            crossing = (x1 - x0) * (latitude - y0) / (y1 - y0) + x0
            if longitude < crossing:
                inside = not inside
        previous = current
    return inside


def emit_assets(output: Path, bounds: tuple[float, float, float, float], roads: list[Road],
                labels: list[Label], green: list[list[tuple[float, float]]],
                source: dict[str, Any], preset: str) -> dict[str, Any]:
    south, west, north, east = bounds
    center_latitude = (south + north) / 2
    tile_lat = min(0.018, north - south)
    tile_lon = min(0.025, east - west)
    columns = max(1, math.ceil((east - west) / tile_lon))
    rows = max(1, math.ceil((north - south) / tile_lat))
    tile_lon = (east - west) / columns
    tile_lat = (north - south) / rows
    tiles: list[list[tuple[int, list[tuple[int, int]]]]] = [[] for _ in range(columns * rows)]

    for road in roads:
        for start, end in zip(road.points, road.points[1:]):
            min_x, max_x = sorted((start[0], end[0]))
            min_y, max_y = sorted((start[1], end[1]))
            if max_x < west or min_x > east or max_y < south or min_y > north:
                continue
            first_col = max(0, int((max(west, min_x) - west) / tile_lon))
            last_col = min(columns - 1, int((min(east - 1e-12, max_x) - west) / tile_lon))
            first_row = max(0, int((max(south, min_y) - south) / tile_lat))
            last_row = min(rows - 1, int((min(north - 1e-12, max_y) - south) / tile_lat))
            for row in range(first_row, last_row + 1):
                for col in range(first_col, last_col + 1):
                    left, bottom = west + col * tile_lon, south + row * tile_lat
                    clipped = clip_segment(*start, *end, left, bottom, left + tile_lon, bottom + tile_lat)
                    if clipped is None:
                        continue
                    ax, ay, bx, by = clipped
                    quantize = lambda lon, lat: (
                        max(0, min(255, round((lon - left) * 255 / tile_lon))),
                        max(0, min(255, round((lat - bottom) * 255 / tile_lat))),
                    )
                    a, b = quantize(ax, ay), quantize(bx, by)
                    if a != b:
                        tiles[row * columns + col].append((road.style, [a, b]))

    blob = bytearray()
    indexes: list[tuple[int, int]] = []
    for tile in tiles:
        start = len(blob)
        for style, points in tile:
            blob.extend((style, len(points)))
            for point in points:
                blob.extend(point)
        indexes.append((start, len(blob) - start))

    label_buckets: dict[int, dict[str, Label]] = defaultdict(dict)
    for label in labels:
        if not (west <= label.longitude < east and south <= label.latitude < north):
            continue
        col = min(columns - 1, int((label.longitude - west) / tile_lon))
        row = min(rows - 1, int((label.latitude - south) / tile_lat))
        key = ascii_name(label.name)
        if key:
            existing = label_buckets[row * columns + col].get(key.casefold())
            if existing is None or label.rank < existing.rank:
                label_buckets[row * columns + col][key.casefold()] = Label(
                    label.longitude, label.latitude, key, label.rank, label.place)

    label_index: list[tuple[int, int]] = []
    label_records: list[tuple[int, int, int, int]] = []
    string_data = bytearray()
    string_offsets: dict[str, int] = {}
    for tile in range(columns * rows):
        start = len(label_records)
        entries = sorted(label_buckets[tile].values(), key=lambda item: (item.rank, item.name))[:6]
        for label in entries:
            if label.name not in string_offsets:
                string_offsets[label.name] = len(string_data)
                string_data.extend(label.name.encode("ascii") + b"\0")
            style = (0x10 if label.place else 0) | min(15, label.rank)
            label_records.append((round(label.longitude * 1e7), round(label.latitude * 1e7),
                                  string_offsets[label.name], style))
        label_index.append((start, len(label_records) - start))

    cell_meters = 260 if preset == "detailed" else 360 if preset == "balanced" else 520
    cell_lat = cell_meters / 110540
    cell_lon = cell_meters / (111320 * math.cos(math.radians(center_latitude)))
    green_columns = max(1, math.ceil((east - west) / cell_lon))
    green_rows = max(1, math.ceil((north - south) / cell_lat))
    cell_lon, cell_lat = (east - west) / green_columns, (north - south) / green_rows
    mask = bytearray((green_columns * green_rows + 7) // 8)
    for row in range(green_rows):
        latitude = south + (row + 0.5) * cell_lat
        for col in range(green_columns):
            longitude = west + (col + 0.5) * cell_lon
            if any(contains(ring, longitude, latitude) for ring in green):
                bit = row * green_columns + col
                mask[bit >> 3] |= 1 << (bit & 7)

    output.mkdir(parents=True, exist_ok=True)
    notice = "// Generated from OpenStreetMap data. © OpenStreetMap contributors, ODbL 1.0."
    data_header = ["#pragma once", "#include <Arduino.h>", notice,
                   '#define ORM_MAP_ATTRIBUTION "© OSM"',
                   "struct OrmMapTile { uint32_t offset; uint32_t length; };",
                   f"static constexpr double ORM_MAP_MIN_LON = {west:.8f};",
                   f"static constexpr double ORM_MAP_MIN_LAT = {south:.8f};",
                   f"static constexpr double ORM_MAP_TILE_LON = {tile_lon:.8f};",
                   f"static constexpr double ORM_MAP_TILE_LAT = {tile_lat:.8f};",
                   f"static constexpr uint16_t ORM_MAP_COLS = {columns};",
                   f"static constexpr uint16_t ORM_MAP_ROWS = {rows};",
                   f"static const OrmMapTile ORM_MAP_INDEX[{len(indexes)}] PROGMEM = {{",
                   ",".join(f"{{{offset}u,{length}u}}" for offset, length in indexes), "};",
                   f"static const uint8_t ORM_MAP_DATA[{max(1, len(blob))}] PROGMEM = {{",
                   ",".join(map(str, blob)) or "0", "};",
                   f"static constexpr uint32_t ORM_MAP_FEATURES = {sum(map(len, tiles))}u;",
                   f"static constexpr uint32_t ORM_MAP_BYTES = {len(blob)}u;"]
    (output / "OrmMapData.h").write_text("\n".join(data_header) + "\n")

    labels_header = ["#pragma once", "#include <Arduino.h>", notice,
                     "struct __attribute__((packed)) OrmMapLabelTile { uint16_t start; uint8_t count; };",
                     "struct __attribute__((packed)) OrmMapLabel { int32_t lonE7; int32_t latE7; uint32_t text; uint8_t style; };",
                     f"static const OrmMapLabelTile ORM_LABEL_INDEX[{len(label_index)}] PROGMEM = {{",
                     ",".join(f"{{{start},{count}}}" for start, count in label_index), "};",
                     f"static const OrmMapLabel ORM_LABELS[{max(1, len(label_records))}] PROGMEM = {{",
                     ",".join("{%d,%d,%d,%d}" % item for item in label_records) or "{0,0,0,0}", "};",
                     f"static const char ORM_LABEL_TEXT[{max(1, len(string_data))}] PROGMEM = {{",
                     ",".join(map(str, string_data)) or "0", "};",
                     f"static constexpr uint16_t ORM_LABEL_COUNT = {len(label_records)};",
                     f"static constexpr uint32_t ORM_LABEL_BYTES = {len(string_data)};"]
    (output / "OrmMapLabels.h").write_text("\n".join(labels_header) + "\n")

    green_header = ["#pragma once", "#include <Arduino.h>", notice,
                    f"static constexpr double ORM_GREEN_MIN_LON = {west:.8f};",
                    f"static constexpr double ORM_GREEN_MIN_LAT = {south:.8f};",
                    f"static constexpr double ORM_GREEN_CELL_LON = {cell_lon:.8f};",
                    f"static constexpr double ORM_GREEN_CELL_LAT = {cell_lat:.8f};",
                    f"static constexpr uint16_t ORM_GREEN_COLS = {green_columns};",
                    f"static constexpr uint16_t ORM_GREEN_ROWS = {green_rows};",
                    f"static const uint8_t ORM_GREEN_MASK[{len(mask)}] PROGMEM = {{",
                    ",".join(map(str, mask)), "};"]
    (output / "OrmGreenMask.h").write_text("\n".join(green_header) + "\n")

    preview = {"schema_version": 1, "bounds": [south, west, north, east],
               "roads": [{"style": road.style, "name": road.name, "points": road.points} for road in roads],
               "labels": [label.__dict__ for label in labels], "green": green,
               "attribution": "© OpenStreetMap contributors" if source["kind"].startswith("overpass") else "Synthetic sample"}
    preview_text = json.dumps(preview, ensure_ascii=False, separators=(",", ":"), sort_keys=True)
    (output / "map-preview.json").write_text(preview_text + "\n")
    manifest = {"schema_version": 1, "created_unix": int(os.environ.get("SOURCE_DATE_EPOCH", "0")), "bounds": preview["bounds"],
                "preset": preset, "source": source, "road_count": len(roads),
                "label_count": len(label_records), "green_polygon_count": len(green),
                "map_bytes": len(blob), "label_bytes": len(string_data), "green_mask_bytes": len(mask),
                "preview_sha256": hashlib.sha256(preview_text.encode()).hexdigest(),
                "attribution": preview["attribution"]}
    (output / "map-manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    return manifest


def build(config: dict[str, Any], offline: bool = False) -> tuple[Path, dict[str, Any]]:
    bounds = bounds_from_config(config)
    map_config = config.get("map", {})
    preset = map_config.get("preset", "balanced")
    if map_config.get("area_type", "sample") == "sample":
        roads, labels, green, source = load_sample()
    else:
        raw, source = fetch_overpass(bounds, offline, map_config.get("overpass_endpoint", "https://overpass-api.de/api/interpreter"))
        roads, labels, green = parse_overpass(raw, preset)
    output = state_dir() / "generated" / "map"
    return output, emit_assets(output, bounds, roads, labels, green, source, preset)

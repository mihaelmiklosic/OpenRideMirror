const $ = (id) => document.getElementById(id);
const form = $("builder");
const status = $("status");
const downloadButton = $("download");
const canvas = $("preview");
const ctx = canvas.getContext("2d", { alpha: false });
let currentPack = null;

const MAJOR = new Set(["motorway", "motorway_link", "trunk", "trunk_link", "primary", "primary_link"]);
const SECONDARY = new Set(["secondary", "secondary_link", "tertiary", "tertiary_link"]);
const STREET = new Set(["residential", "unclassified", "living_street", "road"]);
const PATH = new Set(["track", "path", "footway", "pedestrian", "bridleway", "steps"]);
const GREEN = new Set(["forest", "grass", "meadow", "recreation_ground", "village_green"]);
const PAPER = "#edf0ea";
const INK = "#111";

const SAMPLE = {
  roads: [
    { style: 0, name: "Zagrebacka avenija", points: [[15.900,45.796],[15.925,45.796],[15.950,45.797],[15.975,45.800],[16.000,45.802]] },
    { style: 1, name: "Selska cesta", points: [[15.914,45.750],[15.918,45.770],[15.921,45.790],[15.923,45.810],[15.925,45.820]] },
    { style: 1, name: "Horvacanska cesta", points: [[15.900,45.783],[15.925,45.784],[15.950,45.785],[15.975,45.785],[16.000,45.786]] },
    { style: 2, name: "Aleja M. Ljubeka", points: [[15.912,45.776],[15.925,45.780],[15.940,45.782],[15.956,45.783]] },
    { style: 3, name: "Jarun cycle path", points: [[15.916,45.776],[15.928,45.770],[15.942,45.770],[15.955,45.776],[15.942,45.782],[15.926,45.782],[15.916,45.776]] },
    { style: 6, name: "Jarunsko jezero", points: [[15.918,45.778],[15.930,45.773],[15.944,45.773],[15.954,45.777],[15.945,45.781],[15.930,45.781],[15.918,45.778]] },
  ],
  green: [[[15.905,45.765],[15.960,45.765],[15.960,45.787],[15.905,45.787],[15.905,45.765]]],
  places: [{ name: "Jarun", point: [15.931,45.777], rank: 2 }],
};

function asciiName(value) {
  return String(value || "")
    .replaceAll("Đ", "D").replaceAll("đ", "d").replaceAll("Ł", "L").replaceAll("ł", "l")
    .normalize("NFKD").replace(/[\u0300-\u036f]/g, "").replaceAll('"', "")
    .replace(/\s+/g, " ").trim().slice(0, 22);
}

function styleFor(tags, preset) {
  const highway = tags.highway;
  if (MAJOR.has(highway)) return 0;
  if (SECONDARY.has(highway)) return 1;
  if (STREET.has(highway)) return 2;
  if (highway === "cycleway") return 3;
  if (preset !== "minimal" && PATH.has(highway)) return 4;
  if (preset === "detailed" && highway === "service" && tags.service !== "parking_aisle") return 5;
  if (tags.waterway) return 6;
  return null;
}

function selectedBounds() {
  const kind = $("area-type").value;
  if (kind === "sample") return [45.75, 15.90, 45.82, 16.00];
  if (kind === "center-radius") {
    const latitude = Number($("latitude").value);
    const longitude = Number($("longitude").value);
    const radius = Number($("radius").value);
    if (!(latitude >= -90 && latitude <= 90)) throw new Error("Latitude must be between -90 and 90.");
    if (!(longitude >= -180 && longitude <= 180)) throw new Error("Longitude must be between -180 and 180.");
    if (!(radius >= .5 && radius <= 15)) throw new Error("Radius must be between 0.5 and 15 km.");
    const longitudeRadius = radius / (111.32 * Math.cos(latitude * Math.PI / 180));
    return [latitude - radius / 110.54, longitude - longitudeRadius, latitude + radius / 110.54, longitude + longitudeRadius];
  }
  const bounds = ["south", "west", "north", "east"].map((id) => Number($(id).value));
  if (!bounds.every(Number.isFinite)) throw new Error("Enter all four bounding-box coordinates.");
  const [south, west, north, east] = bounds;
  if (!(-90 <= south && south < north && north <= 90)) throw new Error("South must be below north.");
  if (!(-180 <= west && west < east && east <= 180)) throw new Error("West must be left of east.");
  if ((north - south) * (east - west) > .035) throw new Error("That box is too large. Choose a smaller riding area.");
  return bounds;
}

function overpassQuery([south, west, north, east]) {
  const box = [south, west, north, east].map((value) => value.toFixed(7)).join(",");
  return `[out:json][timeout:90];\n(\n  way["highway"](${box});\n  way["waterway"](${box});\n  way["leisure"="park"](${box});\n  way["landuse"~"forest|grass|meadow|recreation_ground|village_green"](${box});\n  way["natural"="wood"](${box});\n  node["place"](${box});\n);\nout tags center geom;`;
}

async function fetchOverpass(bounds) {
  const body = new URLSearchParams({ data: overpassQuery(bounds) });
  const endpoints = ["https://overpass-api.de/api/interpreter", "https://overpass.kumi.systems/api/interpreter"];
  let lastError = null;
  for (const endpoint of endpoints) {
    try {
      const response = await fetch(endpoint, { method: "POST", body });
      if (!response.ok) throw new Error(`HTTP ${response.status}`);
      const data = await response.json();
      if (!Array.isArray(data.elements)) throw new Error("Unexpected Overpass response.");
      if (data.elements.length > 100000) throw new Error("The selected area returned too much data. Choose a smaller area.");
      return { data, endpoint };
    } catch (error) {
      lastError = error;
    }
  }
  throw new Error(`OpenStreetMap request failed (${lastError?.message || "network error"}). Try again or choose the sample map.`);
}

function parseSample() {
  const roads = SAMPLE.roads.map((road) => ({ ...road, points: road.points.map((point) => [...point]) }));
  const labels = roads.filter((road) => road.name).map((road) => {
    const point = road.points[Math.floor(road.points.length / 2)];
    return { longitude: point[0], latitude: point[1], name: road.name, rank: Math.min(5, road.style), place: false };
  });
  labels.push(...SAMPLE.places.map((place) => ({ longitude: place.point[0], latitude: place.point[1], name: place.name, rank: place.rank, place: true })));
  return { roads, labels, green: SAMPLE.green.map((ring) => ring.map((point) => [...point])) };
}

function parseOverpass(source, preset) {
  const roads = [];
  const labels = [];
  const green = [];
  for (const element of source.elements) {
    const tags = element.tags || {};
    const geometry = (element.geometry || []).filter((point) => Number.isFinite(point.lon) && Number.isFinite(point.lat)).map((point) => [point.lon, point.lat]);
    const style = styleFor(tags, preset);
    const name = asciiName(tags.name);
    if (style !== null && geometry.length >= 2) {
      roads.push({ style, points: geometry, name });
      if (name) {
        const point = geometry[Math.floor(geometry.length / 2)];
        labels.push({ longitude: point[0], latitude: point[1], name, rank: Math.min(style, 5), place: false });
      }
    }
    const isGreen = tags.leisure === "park" || GREEN.has(tags.landuse) || tags.natural === "wood";
    if (isGreen && geometry.length >= 4) {
      if (geometry[0][0] !== geometry.at(-1)[0] || geometry[0][1] !== geometry.at(-1)[1]) geometry.push([...geometry[0]]);
      green.push(geometry);
    }
    if (tags.place && name) {
      const center = element.center || element;
      if (Number.isFinite(center.lon) && Number.isFinite(center.lat)) {
        const ranks = { city: 0, town: 1, suburb: 2, village: 3 };
        labels.push({ longitude: center.lon, latitude: center.lat, name, rank: ranks[tags.place] ?? 4, place: true });
      }
    }
  }
  return { roads, labels, green };
}

function clipSegment(x0, y0, x1, y1, left, bottom, right, top) {
  const dx = x1 - x0, dy = y1 - y0;
  const p = [-dx, dx, -dy, dy], q = [x0 - left, right - x0, y0 - bottom, top - y0];
  let u0 = 0, u1 = 1;
  for (let i = 0; i < 4; i += 1) {
    if (p[i] === 0) { if (q[i] < 0) return null; continue; }
    const ratio = q[i] / p[i];
    if (p[i] < 0) { if (ratio > u1) return null; u0 = Math.max(u0, ratio); }
    else { if (ratio < u0) return null; u1 = Math.min(u1, ratio); }
  }
  return [x0 + u0 * dx, y0 + u0 * dy, x0 + u1 * dx, y0 + u1 * dy];
}

function contains(ring, longitude, latitude) {
  let inside = false;
  let previous = ring.at(-1);
  for (const current of ring) {
    const [x0, y0] = previous, [x1, y1] = current;
    if ((y0 > latitude) !== (y1 > latitude)) {
      const crossing = (x1 - x0) * (latitude - y0) / (y1 - y0) + x0;
      if (longitude < crossing) inside = !inside;
    }
    previous = current;
  }
  return inside;
}

function emitAssets(bounds, parsed, preset, osmSource) {
  const [south, west, north, east] = bounds;
  const centerLatitude = (south + north) / 2;
  let tileLat = Math.min(.018, north - south), tileLon = Math.min(.025, east - west);
  const columns = Math.max(1, Math.ceil((east - west) / tileLon));
  const rows = Math.max(1, Math.ceil((north - south) / tileLat));
  tileLon = (east - west) / columns;
  tileLat = (north - south) / rows;
  const tiles = Array.from({ length: columns * rows }, () => []);

  for (const road of parsed.roads) {
    for (let i = 1; i < road.points.length; i += 1) {
      const start = road.points[i - 1], end = road.points[i];
      const minX = Math.min(start[0], end[0]), maxX = Math.max(start[0], end[0]);
      const minY = Math.min(start[1], end[1]), maxY = Math.max(start[1], end[1]);
      if (maxX < west || minX > east || maxY < south || minY > north) continue;
      const firstCol = Math.max(0, Math.floor((Math.max(west, minX) - west) / tileLon));
      const lastCol = Math.min(columns - 1, Math.floor((Math.min(east - 1e-12, maxX) - west) / tileLon));
      const firstRow = Math.max(0, Math.floor((Math.max(south, minY) - south) / tileLat));
      const lastRow = Math.min(rows - 1, Math.floor((Math.min(north - 1e-12, maxY) - south) / tileLat));
      for (let row = firstRow; row <= lastRow; row += 1) for (let col = firstCol; col <= lastCol; col += 1) {
        const left = west + col * tileLon, bottom = south + row * tileLat;
        const clipped = clipSegment(start[0], start[1], end[0], end[1], left, bottom, left + tileLon, bottom + tileLat);
        if (!clipped) continue;
        const quantize = (lon, lat) => [Math.max(0, Math.min(255, Math.round((lon - left) * 255 / tileLon))), Math.max(0, Math.min(255, Math.round((lat - bottom) * 255 / tileLat)))];
        const a = quantize(clipped[0], clipped[1]), b = quantize(clipped[2], clipped[3]);
        if (a[0] !== b[0] || a[1] !== b[1]) tiles[row * columns + col].push([road.style, a, b]);
      }
    }
  }

  const blob = [], indexes = [];
  for (const tile of tiles) {
    const start = blob.length;
    for (const [style, a, b] of tile) blob.push(style, 2, a[0], a[1], b[0], b[1]);
    indexes.push([start, blob.length - start]);
  }
  if (blob.length > 2500000) throw new Error("The generated road map is too large. Choose a smaller area or lower detail.");

  const labelBuckets = Array.from({ length: columns * rows }, () => new Map());
  if ($("labels").checked) for (const label of parsed.labels) {
    if (!(west <= label.longitude && label.longitude < east && south <= label.latitude && label.latitude < north)) continue;
    const col = Math.min(columns - 1, Math.floor((label.longitude - west) / tileLon));
    const row = Math.min(rows - 1, Math.floor((label.latitude - south) / tileLat));
    const name = asciiName(label.name), key = name.toLowerCase();
    if (!name) continue;
    const existing = labelBuckets[row * columns + col].get(key);
    if (!existing || label.rank < existing.rank) labelBuckets[row * columns + col].set(key, { ...label, name });
  }

  const labelIndex = [], labelRecords = [], stringData = [], stringOffsets = new Map();
  const encoder = new TextEncoder();
  for (const bucket of labelBuckets) {
    const start = labelRecords.length;
    const entries = [...bucket.values()].sort((a, b) => a.rank - b.rank || a.name.localeCompare(b.name)).slice(0, 6);
    for (const label of entries) {
      if (!stringOffsets.has(label.name)) {
        stringOffsets.set(label.name, stringData.length);
        stringData.push(...encoder.encode(label.name), 0);
      }
      const style = (label.place ? 0x10 : 0) | Math.min(15, label.rank);
      labelRecords.push([Math.round(label.longitude * 1e7), Math.round(label.latitude * 1e7), stringOffsets.get(label.name), style]);
    }
    labelIndex.push([start, labelRecords.length - start]);
  }

  const cellMeters = preset === "detailed" ? 260 : preset === "balanced" ? 360 : 520;
  let cellLat = cellMeters / 110540, cellLon = cellMeters / (111320 * Math.cos(centerLatitude * Math.PI / 180));
  const greenColumns = Math.max(1, Math.ceil((east - west) / cellLon));
  const greenRows = Math.max(1, Math.ceil((north - south) / cellLat));
  cellLon = (east - west) / greenColumns;
  cellLat = (north - south) / greenRows;
  const mask = new Uint8Array(Math.ceil(greenColumns * greenRows / 8));
  if ($("green").checked) for (let row = 0; row < greenRows; row += 1) {
    const latitude = south + (row + .5) * cellLat;
    for (let col = 0; col < greenColumns; col += 1) {
      const longitude = west + (col + .5) * cellLon;
      if (parsed.green.some((ring) => contains(ring, longitude, latitude))) {
        const bit = row * greenColumns + col;
        mask[bit >> 3] |= 1 << (bit & 7);
      }
    }
  }

  const notice = osmSource ? "// Generated from OpenStreetMap data. © OpenStreetMap contributors, ODbL 1.0." : "// Synthetic OpenRideMirror sample map.";
  const attribution = osmSource ? "© OSM" : "SAMPLE";
  const lines = (...items) => `${items.join("\n")}\n`;
  const mapHeader = lines(
    "#pragma once", "#include <Arduino.h>", notice, `#define ORM_MAP_ATTRIBUTION "${attribution}"`,
    "struct OrmMapTile { uint32_t offset; uint32_t length; };",
    `static constexpr double ORM_MAP_MIN_LON = ${west.toFixed(8)};`, `static constexpr double ORM_MAP_MIN_LAT = ${south.toFixed(8)};`,
    `static constexpr double ORM_MAP_TILE_LON = ${tileLon.toFixed(8)};`, `static constexpr double ORM_MAP_TILE_LAT = ${tileLat.toFixed(8)};`,
    `static constexpr uint16_t ORM_MAP_COLS = ${columns};`, `static constexpr uint16_t ORM_MAP_ROWS = ${rows};`,
    `static const OrmMapTile ORM_MAP_INDEX[${indexes.length}] PROGMEM = {`, indexes.map(([offset, length]) => `{${offset}u,${length}u}`).join(","), "};",
    `static const uint8_t ORM_MAP_DATA[${Math.max(1, blob.length)}] PROGMEM = {`, blob.join(",") || "0", "};",
    `static constexpr uint32_t ORM_MAP_FEATURES = ${tiles.reduce((sum, tile) => sum + tile.length, 0)}u;`, `static constexpr uint32_t ORM_MAP_BYTES = ${blob.length}u;`
  );
  const labelsHeader = lines(
    "#pragma once", "#include <Arduino.h>", notice,
    "struct __attribute__((packed)) OrmMapLabelTile { uint16_t start; uint8_t count; };",
    "struct __attribute__((packed)) OrmMapLabel { int32_t lonE7; int32_t latE7; uint32_t text; uint8_t style; };",
    `static const OrmMapLabelTile ORM_LABEL_INDEX[${labelIndex.length}] PROGMEM = {`, labelIndex.map((item) => `{${item.join(",")}}`).join(","), "};",
    `static const OrmMapLabel ORM_LABELS[${Math.max(1, labelRecords.length)}] PROGMEM = {`, labelRecords.map((item) => `{${item.join(",")}}`).join(",") || "{0,0,0,0}", "};",
    `static const char ORM_LABEL_TEXT[${Math.max(1, stringData.length)}] PROGMEM = {`, stringData.join(",") || "0", "};",
    `static constexpr uint16_t ORM_LABEL_COUNT = ${labelRecords.length};`, `static constexpr uint32_t ORM_LABEL_BYTES = ${stringData.length};`
  );
  const greenHeader = lines(
    "#pragma once", "#include <Arduino.h>", notice,
    `static constexpr double ORM_GREEN_MIN_LON = ${west.toFixed(8)};`, `static constexpr double ORM_GREEN_MIN_LAT = ${south.toFixed(8)};`,
    `static constexpr double ORM_GREEN_CELL_LON = ${cellLon.toFixed(8)};`, `static constexpr double ORM_GREEN_CELL_LAT = ${cellLat.toFixed(8)};`,
    `static constexpr uint16_t ORM_GREEN_COLS = ${greenColumns};`, `static constexpr uint16_t ORM_GREEN_ROWS = ${greenRows};`,
    `static const uint8_t ORM_GREEN_MASK[${mask.length}] PROGMEM = {`, [...mask].join(","), "};"
  );
  const manifest = {
    schema_version: 1, generator: "OpenRideMirror web map builder", generated_at: new Date().toISOString(),
    bounds, preset, source: osmSource ? "OpenStreetMap via Overpass" : "Synthetic Jarun sample",
    road_count: parsed.roads.length, label_count: labelRecords.length, green_polygon_count: parsed.green.length,
    map_bytes: blob.length, label_bytes: stringData.length, green_mask_bytes: mask.length,
    attribution: osmSource ? "© OpenStreetMap contributors" : "Synthetic sample",
  };
  return { mapHeader, labelsHeader, greenHeader, manifest, preview: { bounds, ...parsed, osmSource } };
}

function drawPreview(data) {
  const [south, west, north, east] = data.bounds;
  const project = ([lon, lat]) => [(lon - west) * canvas.width / (east - west), canvas.height - (lat - south) * canvas.height / (north - south)];
  ctx.fillStyle = PAPER; ctx.fillRect(0, 0, canvas.width, canvas.height);
  if ($("green").checked) {
    ctx.fillStyle = "#b8bbb5";
    for (const ring of data.green) {
      ctx.beginPath(); ring.forEach((point, index) => { const [x, y] = project(point); index ? ctx.lineTo(x, y) : ctx.moveTo(x, y); }); ctx.fill();
    }
  }
  ctx.lineCap = "round"; ctx.lineJoin = "round";
  for (const road of data.roads) {
    ctx.beginPath(); road.points.forEach((point, index) => { const [x, y] = project(point); index ? ctx.lineTo(x, y) : ctx.moveTo(x, y); });
    ctx.strokeStyle = INK; ctx.lineWidth = road.style === 0 ? 4 : road.style <= 2 ? 2 : 1;
    ctx.setLineDash(road.style === 3 ? [7, 4] : road.style > 3 ? [2, 4] : []); ctx.stroke();
  }
  ctx.setLineDash([]); ctx.font = "bold 10px monospace";
  if ($("labels").checked) for (const label of data.labels.sort((a, b) => a.rank - b.rank).slice(0, 24)) {
    const [x, y] = project([label.longitude, label.latitude]);
    if (x < 2 || x > 270 || y < 10 || y > 390) continue;
    const width = ctx.measureText(label.name).width + 4;
    ctx.fillStyle = PAPER; ctx.fillRect(x - 2, y - 10, width, 12); ctx.fillStyle = INK; ctx.fillText(label.name, x, y);
  }
  ctx.fillStyle = PAPER; ctx.fillRect(2, 384, data.osmSource ? 162 : 96, 14);
  ctx.fillStyle = INK; ctx.font = "bold 9px monospace"; ctx.fillText(data.osmSource ? "© OpenStreetMap contributors" : "SYNTHETIC SAMPLE", 5, 395);
}

function crc32(bytes) {
  let crc = 0xffffffff;
  for (const byte of bytes) {
    crc ^= byte;
    for (let bit = 0; bit < 8; bit += 1) crc = (crc >>> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
  }
  return (crc ^ 0xffffffff) >>> 0;
}

function zipStore(files) {
  const encoder = new TextEncoder(), localParts = [], centralParts = [];
  let offset = 0, centralSize = 0;
  const u16 = (value) => new Uint8Array([value & 255, value >>> 8 & 255]);
  const u32 = (value) => new Uint8Array([value & 255, value >>> 8 & 255, value >>> 16 & 255, value >>> 24 & 255]);
  const join = (parts) => { const result = new Uint8Array(parts.reduce((sum, part) => sum + part.length, 0)); let at = 0; for (const part of parts) { result.set(part, at); at += part.length; } return result; };
  for (const [name, text] of Object.entries(files)) {
    const nameBytes = encoder.encode(name), data = encoder.encode(text), crc = crc32(data);
    const local = join([u32(0x04034b50),u16(20),u16(0),u16(0),u16(0),u16(0),u32(crc),u32(data.length),u32(data.length),u16(nameBytes.length),u16(0),nameBytes,data]);
    const central = join([u32(0x02014b50),u16(20),u16(20),u16(0),u16(0),u16(0),u16(0),u32(crc),u32(data.length),u32(data.length),u16(nameBytes.length),u16(0),u16(0),u16(0),u16(0),u32(0),u32(offset),nameBytes]);
    localParts.push(local); centralParts.push(central); offset += local.length; centralSize += central.length;
  }
  const end = join([u32(0x06054b50),u16(0),u16(0),u16(centralParts.length),u16(centralParts.length),u32(centralSize),u32(offset),u16(0)]);
  return new Blob([...localParts, ...centralParts, end], { type: "application/zip" });
}

async function buildMap() {
  const bounds = selectedBounds(), preset = $("preset").value, kind = $("area-type").value;
  let parsed, sourceDescription;
  if (kind === "sample") {
    parsed = parseSample(); sourceDescription = "Synthetic sample";
  } else {
    status.textContent = "Requesting OpenStreetMap data…";
    const response = await fetchOverpass(bounds);
    status.textContent = `Processing ${response.data.elements.length.toLocaleString()} OpenStreetMap elements…`;
    parsed = parseOverpass(response.data, preset); sourceDescription = response.endpoint;
  }
  const assets = emitAssets(bounds, parsed, preset, kind !== "sample");
  const readme = `OpenRideMirror map pack\n\nSource: ${sourceDescription}\nRoads: ${assets.manifest.road_count}\nMap bytes: ${assets.manifest.map_bytes}\n\nConnect the display, open Terminal in the OpenRideMirror repository folder and run:\n\n  ./orm map flash\n\nThat one command finds the newest ORM map pack in Downloads, validates and installs it, builds live firmware and flashes the connected ESP32.\n`;
  const files = {
    "OrmMapData.h": assets.mapHeader, "OrmMapLabels.h": assets.labelsHeader, "OrmGreenMask.h": assets.greenHeader,
    "map-manifest.json": `${JSON.stringify(assets.manifest, null, 2)}\n`, "README.txt": readme,
  };
  currentPack = zipStore(files);
  drawPreview(assets.preview);
  $("size").textContent = `${Math.ceil(currentPack.size / 1024).toLocaleString()} KB ZIP`;
  downloadButton.disabled = false;
  status.textContent = `Ready: ${assets.manifest.road_count.toLocaleString()} roads, ${assets.manifest.label_count.toLocaleString()} labels.`;
}

form.addEventListener("submit", async (event) => {
  event.preventDefault(); currentPack = null; downloadButton.disabled = true; status.classList.remove("error");
  try { await buildMap(); }
  catch (error) { status.textContent = error.message; status.classList.add("error"); }
});

downloadButton.addEventListener("click", () => {
  if (!currentPack) return;
  const url = URL.createObjectURL(currentPack), link = document.createElement("a");
  link.href = url; link.download = "OpenRideMirror-map-pack.zip"; link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
});

$("area-type").addEventListener("change", () => {
  const kind = $("area-type").value;
  $("center-options").hidden = kind !== "center-radius";
  $("bbox-options").hidden = kind !== "bbox";
  currentPack = null; downloadButton.disabled = true; $("size").textContent = "—";
});

ctx.fillStyle = PAPER; ctx.fillRect(0, 0, canvas.width, canvas.height);
ctx.fillStyle = INK; ctx.font = "bold 13px monospace"; ctx.textAlign = "center";
ctx.fillText("BUILD A MAP TO PREVIEW", canvas.width / 2, canvas.height / 2);
ctx.textAlign = "start";

const canvas = document.getElementById("dashboard");
const ctx = canvas.getContext("2d", { alpha: false });
ctx.imageSmoothingEnabled = false;

const PAPER = "#edf0ea";
const INK = "#090909";
const route = [
  [15.9217, 45.7840], [15.9242, 45.7848], [15.9270, 45.7851],
  [15.9293, 45.7864], [15.9323, 45.7867], [15.9352, 45.7880],
  [15.9382, 45.7886], [15.9411, 45.7900], [15.9441, 45.7905],
  [15.9470, 45.7919],
];

let fonts = {};
let selectedView = "ride";
let cachedMap = null;
let cachedMapKey = "";

function parseBdf(source) {
  const glyphs = new Map();
  const lines = source.split(/\r?\n/);
  for (let i = 0; i < lines.length; i += 1) {
    if (!lines[i].startsWith("STARTCHAR")) continue;
    const glyph = { encoding: -1, advance: 0, width: 0, height: 0, xOffset: 0, yOffset: 0, rows: [] };
    while (++i < lines.length && lines[i] !== "ENDCHAR") {
      const parts = lines[i].split(/\s+/);
      if (parts[0] === "ENCODING") glyph.encoding = Number(parts[1]);
      else if (parts[0] === "DWIDTH") glyph.advance = Number(parts[1]);
      else if (parts[0] === "BBX") [glyph.width, glyph.height, glyph.xOffset, glyph.yOffset] = parts.slice(1).map(Number);
      else if (parts[0] === "BITMAP") for (let row = 0; row < glyph.height; row += 1) glyph.rows.push(lines[++i].trim());
    }
    if (glyph.encoding >= 0) glyphs.set(glyph.encoding, glyph);
  }
  return glyphs;
}

function textWidth(text, font) {
  let width = 0;
  for (const character of text) width += font.get(character.charCodeAt(0))?.advance ?? 6;
  return width;
}

function drawText(text, x, baseline, font, options = {}) {
  if (options.align === "center") x -= Math.floor(textWidth(text, font) / 2);
  if (options.align === "right") x -= textWidth(text, font);
  ctx.fillStyle = options.color || INK;
  for (const character of text) {
    const glyph = font.get(character.charCodeAt(0));
    if (!glyph) { x += 6; continue; }
    const top = baseline - glyph.height - glyph.yOffset;
    for (let row = 0; row < glyph.height; row += 1) {
      const bits = BigInt(`0x${glyph.rows[row] || "0"}`);
      const padded = Math.ceil(glyph.width / 8) * 8;
      for (let column = 0; column < glyph.width; column += 1) {
        if ((bits & (1n << BigInt(padded - 1 - column))) !== 0n) ctx.fillRect(Math.round(x + glyph.xOffset + column), Math.round(top + row), 1, 1);
      }
    }
    x += glyph.advance;
  }
}

function drawRasterText(text, x, baseline, size, options = {}) {
  const scale = 2;
  const scratch = document.createElement("canvas");
  scratch.width = 700; scratch.height = 220;
  const s = scratch.getContext("2d");
  s.fillStyle = "white"; s.font = `700 ${size * scale}px OrmInconsolata`; s.textBaseline = "alphabetic";
  const localBaseline = size;
  s.fillText(text, 8, localBaseline * scale);
  const source = s.getImageData(0, 0, scratch.width, scratch.height);
  const width = Math.ceil(s.measureText(text).width / scale) + 6;
  if (options.align === "center") x -= Math.floor(width / 2);
  const top = Math.max(0, baseline - size);
  ctx.fillStyle = options.color || INK;
  for (let py = top; py <= baseline + 4; py += 1) for (let px = 0; px < width; px += 1) {
    let coverage = 0;
    const localY = py - top;
    for (let sy = 0; sy < scale; sy += 1) for (let sx = 0; sx < scale; sx += 1) coverage += source.data[((localY * scale + sy) * scratch.width + px * scale + sx) * 4 + 3];
    if (coverage >= 300) ctx.fillRect(Math.round(x + px), py, 1, 1);
  }
}

function rasterWidth(text, size) {
  const scratch = document.createElement("canvas").getContext("2d");
  scratch.font = `700 ${size * 2}px OrmInconsolata`;
  return Math.ceil(scratch.measureText(text).width / 2);
}

function drawTightSpeed(speed, baseline, leftX = 10, color = INK, centered = false) {
  const [integer, fraction = "0"] = speed.toFixed(1).split(".");
  const integerWidth = rasterWidth(integer, 84), fractionWidth = rasterWidth(fraction, 84);
  const totalWidth = integerWidth + 5 + 7 + 5 + fractionWidth;
  const x = centered ? Math.floor((300 - totalWidth) / 2) : leftX;
  drawRasterText(integer, x, baseline, 84, { color });
  const decimalX = x + integerWidth + 5;
  ctx.fillStyle = color;
  ctx.beginPath(); ctx.arc(decimalX + 3, baseline - 4, 3, 0, Math.PI * 2); ctx.fill();
  drawRasterText(fraction, decimalX + 12, baseline, 84, { color });
}

const hash = (value) => {
  value = (value ^ (value >>> 16)) >>> 0; value = Math.imul(value, 0x7feb352d) >>> 0;
  value = (value ^ (value >>> 15)) >>> 0; value = Math.imul(value, 0x846ca68b) >>> 0;
  return (value ^ (value >>> 16)) >>> 0;
};
const pad = (value) => String(value).padStart(2, "0");
const duration = (seconds) => `${pad(Math.floor(seconds / 3600))}:${pad(Math.floor(seconds / 60) % 60)}:${pad(Math.floor(seconds) % 60)}`;
const zoneFor = (bpm) => bpm < 105 ? 1 : bpm < 125 ? 2 : bpm < 145 ? 3 : bpm < 165 ? 4 : 5;

function speedFor(seconds) {
  const t = seconds % 180;
  if (t < 8) return 17 + t * 2.05;
  if (t < 24) return 33.4 - (t - 8) * 0.34;
  if (t < 52) return 24.5 + Math.sin((t - 24) * 0.18) * 3.1;
  if (t < 62) return 27 + (t - 52) * 0.68;
  if (t < 78) return 33.8 - (t - 62) * 0.38;
  if (t < 110) return 22.5 + Math.sin(t * 0.25) * 2.8;
  if (t < 123) return 24 + (t - 110) * 0.72;
  if (t < 140) return 33.4 - (t - 123) * 0.31;
  return 23 + Math.sin(t * 0.17) * 3.7;
}

function routePosition(progress) {
  const scaled = Math.min(route.length - 1.001, progress * (route.length - 1));
  const index = Math.floor(scaled), amount = scaled - index;
  return [route[index][0] + (route[index + 1][0] - route[index][0]) * amount, route[index][1] + (route[index + 1][1] - route[index][1]) * amount];
}

function drawHeart(x, y) {
  ctx.fillStyle = INK;
  for (let py = 0; py <= 16; py += 1) for (let px = 0; px <= 15; px += 1) {
    const leftDisc = (px - 4) ** 2 + (py - 4) ** 2 <= 16;
    const rightDisc = (px - 11) ** 2 + (py - 4) ** 2 <= 16;
    const triangle = py >= 5 && py <= 16 && px >= Math.ceil((py - 5) * 7 / 11) && px <= Math.floor(15 - (py - 5) * 8 / 11);
    if (leftDisc || rightDisc || triangle) ctx.fillRect(x + px, y + py, 1, 1);
  }
}

function drawNoise(now, speed) {
  ctx.fillStyle = PAPER; ctx.fillRect(0, 0, 300, 18); ctx.fillStyle = INK;
  const phase = now * 0.00028, energy = Math.max(0, Math.min(1, (speed - 17) / 18));
  for (let y = 3; y <= 15; y += 3) for (let x = 3; x < 299; x += 3) {
    const n = (Math.sin(x * .23 + phase) + Math.sin(y * .67 - phase * .71) + Math.sin((x + y) * .11 + phase * .43)) / 6 + .5;
    if (n > .46) { const size = n > .72 - energy * .04 ? 2 : 1; ctx.fillRect(x, y, size, size); }
  }
}

function pixelLine(target, x0, y0, x1, y1, width = 1, dash = 0) {
  x0 = Math.round(x0); y0 = Math.round(y0); x1 = Math.round(x1); y1 = Math.round(y1);
  const dx = Math.abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -Math.abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  let error = dx + dy, step = 0;
  while (true) {
    if (!dash || step % dash < Math.max(1, dash - 2)) target.fillRect(x0 - Math.floor(width / 2), y0 - Math.floor(width / 2), width, width);
    if (x0 === x1 && y0 === y1) break;
    const twice = 2 * error;
    if (twice >= dy) { error += dy; x0 += sx; }
    if (twice <= dx) { error += dx; y0 += sy; }
    step += 1;
  }
}

function buildMap(centerLon, centerLat, progress) {
  const mapCanvas = document.createElement("canvas"); mapCanvas.width = 300; mapCanvas.height = 122;
  const m = mapCanvas.getContext("2d", { alpha: false }); m.fillStyle = PAPER; m.fillRect(0, 0, 300, 122); m.fillStyle = INK;
  const lonRange = 3000 / (111320 * Math.cos(centerLat * Math.PI / 180)), latRange = 3000 * 122 / 300 / 110540;
  const left = centerLon - lonRange / 2, bottom = centerLat - latRange / 2;
  const project = ([lon, lat]) => [(lon - left) * 300 / lonRange, 122 - (lat - bottom) * 122 / latRange];
  const greenSet = new Set(window.ORM_MAP.greenCells.map(([col, row]) => `${col}:${row}`)), g = window.ORM_MAP.green;
  for (let y = 6; y < 119; y += 7) for (let x = 5; x < 297; x += 7) {
    const lon = left + x * lonRange / 300, lat = bottom + (122 - y) * latRange / 122;
    const col = Math.floor((lon - g.minLon) / g.cellLon), row = Math.floor((lat - g.minLat) / g.cellLat);
    if (!greenSet.has(`${col}:${row}`)) continue;
    const bit = row * g.cols + col; m.fillRect(x, y, 1, 1);
    if ((hash(bit) & 15) === 0) { m.fillRect(x - 1, y, 3, 1); m.fillRect(x, y - 1, 1, 3); }
  }
  for (const [style, points] of window.ORM_MAP.features) for (let i = 1; i < points.length; i += 1) {
    const a = project(points[i - 1]), b = project(points[i]);
    if ((a[0] < -4 && b[0] < -4) || (a[0] > 304 && b[0] > 304) || (a[1] < -4 && b[1] < -4) || (a[1] > 126 && b[1] > 126)) continue;
    pixelLine(m, a[0], a[1], b[0], b[1], style === 0 ? 3 : style <= 2 ? 2 : 1, style === 3 ? 5 : style === 6 ? 7 : style > 2 ? 4 : 0);
  }
  m.fillStyle = PAPER;
  for (let i = 1; i < route.length; i += 1) { const a = project(route[i - 1]), b = project(route[i]); pixelLine(m, a[0], a[1], b[0], b[1], 7); }
  m.fillStyle = INK;
  const visibleRoute = Math.max(1, Math.ceil(progress * (route.length - 1)));
  for (let i = 1; i <= visibleRoute; i += 1) { const a = project(route[i - 1]), b = project(route[i]); pixelLine(m, a[0], a[1], b[0], b[1], 3); }
  const candidates = window.ORM_MAP.labels.map((label) => {
    const point = project(label), rank = label[3] & 15, place = (label[3] & 16) !== 0;
    return { x: point[0], y: point[1], text: label[2].slice(0, 22), score: rank * 10000 + (place ? 0 : 1200) + (point[0] - 150) ** 2 + (point[1] - 61) ** 2 };
  }).filter((item) => item.x >= 0 && item.x < 300 && item.y >= 0 && item.y < 122).sort((a, b) => a.score - b.score);
  const boxes = [];
  for (const label of candidates) {
    if (boxes.length >= 5) break;
    const width = Math.min(294, textWidth(label.text, fonts.small) + 8), leftBox = Math.max(2, Math.min(298 - width, Math.round(label.x - width / 2))), topBox = Math.max(7, Math.min(107, Math.round(label.y - 6)));
    const box = [leftBox, topBox, leftBox + width, topBox + 13];
    if (boxes.some((old) => box[0] <= old[2] + 2 && box[2] + 2 >= old[0] && box[1] <= old[3] + 1 && box[3] + 1 >= old[1])) continue;
    m.fillStyle = PAPER; m.fillRect(leftBox, topBox, width, 13); boxes.push([...box, label.text]);
  }
  const position = project(routePosition(progress));
  m.fillStyle = PAPER; m.beginPath(); m.arc(Math.round(position[0]), Math.round(position[1]), 7, 0, Math.PI * 2); m.fill();
  m.fillStyle = INK; m.beginPath(); m.arc(Math.round(position[0]), Math.round(position[1]), 4, 0, Math.PI * 2); m.fill();
  m.fillStyle = PAPER; m.fillRect(Math.round(position[0]) - 1, Math.round(position[1]) - 1, 3, 3);
  return { canvas: mapCanvas, labels: boxes };
}

function drawMap(centerLon, centerLat, progress) {
  const key = `${centerLon.toFixed(4)}:${centerLat.toFixed(4)}:${Math.floor(progress * 80)}`;
  if (!cachedMap || key !== cachedMapKey) { cachedMap = buildMap(centerLon, centerLat, progress); cachedMapKey = key; }
  ctx.drawImage(cachedMap.canvas, 0, 278);
  for (const [left, top, , , text] of cachedMap.labels) drawText(text, left + 4, 278 + top + 10, fonts.small);
  ctx.fillStyle = PAPER; ctx.fillRect(2, 386, 34, 12);
  drawText("© OSM", 4, 396, fonts.small);
}

function drawDashboard(data, now) {
  ctx.fillStyle = PAPER; ctx.fillRect(0, 0, 300, 400); drawNoise(now, data.speed); ctx.fillStyle = INK;
  [[0,18,300,1],[198,18,1,104],[198,70,102,1],[0,122,300,1],[99,122,1,52],[198,122,1,52],[0,174,300,1],[99,174,1,104],[198,174,1,104],[0,226,300,1],[0,278,300,1]].forEach(([x,y,w,h]) => ctx.fillRect(x,y,w,h));
  drawText("SPEED KM/H", 10, 37, fonts.small); drawTightSpeed(data.speed, 113, 10);
  drawText("AVG SPEED", 207, 36, fonts.small); drawText(data.avgSpeed.toFixed(1), 212, 62, fonts.large);
  drawText("DIST KM", 212, 88, fonts.small); drawText(data.distance.toFixed(1), 212, 114, fonts.large);
  drawHeart(14, 142); drawText(String(data.bpm), 37, 158, fonts.large); drawText(`ZONE ${zoneFor(data.bpm)}`, 112, 158, fonts.large);
  drawText("AVG HR", 212, 139, fonts.small); drawText(String(data.avgHr), 212, 163, fonts.large);
  const cells = [[14,"TIME",data.clock],[113,"TEMP",`${data.temp.toFixed(0)} C`],[212,"ASCENT",`${data.ascent} M`],[14,"ELAPSED",duration(data.seconds)],[113,"KCAL",String(data.kcal)],[212,"GRADE",`${data.grade >= 0 ? "+" : ""}${Math.round(data.grade)}%`]];
  cells.forEach(([x,label,value], index) => { const offset = index < 3 ? 0 : 52; drawText(label,x,192+offset,fonts.small); drawText(value,x,217+offset,fonts.large); });
  drawMap(data.lon, data.lat, data.progress);
}

function drawPush(now, speed, secondsLeft) {
  ctx.fillStyle = INK; ctx.fillRect(0, 0, 300, 400); const cx = 150, cy = 200, phase = now * .0018; ctx.fillStyle = PAPER;
  for (let ring = 0; ring < 17; ring += 1) for (let ray = 0; ray < 42; ray += 1) {
    const n = Math.sin(ray * .61 + ring * .37 + phase) * .5 + .5;
    if (n < .48) continue;
    const radius = (ring * 18 + phase * 15) % 285, angle = ray * Math.PI * 2 / 42 + Math.sin(ring + phase) * .035, size = n > .84 ? 3 : n > .67 ? 2 : 1;
    ctx.fillRect(Math.round(cx + Math.cos(angle) * radius), Math.round(cy + Math.sin(angle) * radius), size, size);
  }
  ctx.fillStyle = INK; ctx.beginPath(); ctx.arc(cx,cy,135,0,Math.PI*2); ctx.fill();
  drawText("SPEED KM/H",cx,124,fonts.large,{color:PAPER,align:"center"}); drawTightSpeed(speed,222,0,PAPER,true); drawText(String(Math.max(1,Math.ceil(secondsLeft))).padStart(2,"0"),cx,286,fonts.large,{color:PAPER,align:"center"});
}

function frame(now) {
  const progress=.62,[lon,lat]=routePosition(progress);
  const data={seconds:4512,progress,lon,lat,speed:28.7,avgSpeed:24.1,distance:42.7,bpm:152,avgHr:143,temp:17,ascent:318,kcal:612,grade:2,clock:"14:32"};
  if(selectedView === "push") drawPush(now,34.7,7); else drawDashboard(data,now);
  requestAnimationFrame(frame);
}

async function start() {
  const [small,large]=await Promise.all([fetch("fonts/helvB08.bdf").then(r=>r.text()),fetch("fonts/helvB14.bdf").then(r=>r.text())]);
  fonts={small:parseBdf(small),large:parseBdf(large)};
  const inconsolata=new FontFace("OrmInconsolata",`url(${window.ORM_INCONSOLATA_BOLD})`,{weight:"700"}); await inconsolata.load(); document.fonts.add(inconsolata);
  requestAnimationFrame(frame);
}
for (const name of ["ride", "push"]) document.getElementById(`${name}-view`).addEventListener("click", () => {
  selectedView = name;
  for (const candidate of ["ride", "push"]) document.getElementById(`${candidate}-view`).setAttribute("aria-pressed", String(candidate === name));
});
start();

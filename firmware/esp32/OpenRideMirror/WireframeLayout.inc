// SPDX-License-Identifier: GPL-3.0-only
static void wireDrawBattery(int x, int y, uint8_t percent) {
  u8g2->drawFrame(x, y, 12, 7);
  u8g2->drawBox(x + 12, y + 2, 2, 3);
  if (percent != UNKNOWN_U8) {
    int fill = constrain((int)percent * 8 / 100, 0, 8);
    if (fill > 0) u8g2->drawBox(x + 2, y + 2, fill, 3);
  }
}

static void wireDrawHeavy(int x, int y, const char *text) {
  u8g2->drawStr(x, y, text);
}

// The large numeric font is monospaced, so its period normally consumes the
// width of a complete digit and visually separates 34 . 0. Draw the decimal
// point ourselves and keep the fractional digit tight to the integer instead.
// Returns the left edge and reports the hand-drawn decimal x coordinate.
// Pass a non-negative leftX for left alignment or -1 to center the value.
static int wireDrawTightSpeed(float speedKmh, int baseline, int &decimalX,
                              int leftX) {
  char formatted[16];
  char integerPart[12];
  char fraction[2] = {'0', '\0'};
  snprintf(formatted, sizeof(formatted), "%.1f", speedKmh);
  const char *point = strchr(formatted, '.');
  size_t integerLength = point == nullptr
                             ? strlen(formatted)
                             : min((size_t)(point - formatted), sizeof(integerPart) - 1);
  memcpy(integerPart, formatted, integerLength);
  integerPart[integerLength] = '\0';
  if (point != nullptr && point[1] != '\0') fraction[0] = point[1];

  int integerWidth = u8g2->getStrWidth(integerPart);
  int fractionWidth = u8g2->getStrWidth(fraction);
  const int dotGap = 5;
  const int dotDiameter = 7;
  const int fractionGap = 5;
  int totalWidth = integerWidth + dotGap + dotDiameter + fractionGap + fractionWidth;
  int x = leftX >= 0 ? leftX : (300 - totalWidth) / 2;
  u8g2->drawStr(x, baseline, integerPart);
  decimalX = x + integerWidth + dotGap;
  u8g2->drawDisc(decimalX + dotDiameter / 2, baseline - 4, dotDiameter / 2);
  u8g2->drawStr(decimalX + dotDiameter + fractionGap, baseline, fraction);
  return x;
}

static void wireDrawLinkBars(int x, int y, bool connected) {
  for (int bar = 0; bar < 4; ++bar) {
    int height = 2 + bar * 2;
    if (connected) u8g2->drawBox(x + bar * 3, y + 8 - height, 2, height);
    else u8g2->drawFrame(x + bar * 3, y + 8 - height, 2, height);
  }
}

static void wireDrawHeart(int x, int y) {
  u8g2->drawDisc(x + 4, y + 4, 4);
  u8g2->drawDisc(x + 11, y + 4, 4);
  u8g2->drawTriangle(x, y + 5, x + 15, y + 5, x + 7, y + 16);
}

static void wireDrawZoneBadge(int x, int y, uint8_t zone) {
  char badge[6];
  if (zone == UNKNOWN_U8) snprintf(badge, sizeof(badge), "Z-");
  else snprintf(badge, sizeof(badge), "Z%u", zone);
  u8g2->setDrawColor(1);
  u8g2->drawRBox(x, y, 42, 26, 5);
  u8g2->setDrawColor(0);
  u8g2->setFont(u8g2_font_helvB10_tf);
  u8g2->drawStr(x + (42 - u8g2->getStrWidth(badge)) / 2, y + 19, badge);
  u8g2->setDrawColor(1);
}

static uint32_t wireNoiseHash(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  return value ^ (value >> 16);
}

static float wirePerlinFade(float value) {
  return value * value * value * (value * (value * 6.0f - 15.0f) + 10.0f);
}

static float wirePerlinGradient(int gridX, int gridY, float x, float y) {
  float dx = x - gridX;
  float dy = y - gridY;
  uint32_t hash = wireNoiseHash((uint32_t)(gridX + 32768) * 374761393U ^
                                (uint32_t)(gridY + 32768) * 668265263U);
  switch (hash & 7U) {
    case 0: return dx + dy;
    case 1: return dx - dy;
    case 2: return -dx + dy;
    case 3: return -dx - dy;
    case 4: return dx;
    case 5: return -dx;
    case 6: return dy;
    default: return -dy;
  }
}

// True 2D gradient Perlin noise: lattice gradients, dot products and quintic
// fade interpolation. Output is normalized to the 0..1 range.
static float wireSmoothNoise(float x, float y) {
  int x0 = (int)floorf(x);
  int y0 = (int)floorf(y);
  float tx = wirePerlinFade(x - x0);
  float ty = wirePerlinFade(y - y0);
  float topLeft = wirePerlinGradient(x0, y0, x, y);
  float topRight = wirePerlinGradient(x0 + 1, y0, x, y);
  float bottomLeft = wirePerlinGradient(x0, y0 + 1, x, y);
  float bottomRight = wirePerlinGradient(x0 + 1, y0 + 1, x, y);
  float top = topLeft * (1.0f - tx) + topRight * tx;
  float bottom = bottomLeft * (1.0f - tx) + bottomRight * tx;
  return constrain(0.5f + (top * (1.0f - ty) + bottom * ty) * 0.36f,
                   0.0f, 1.0f);
}

static void wireDrawMoodPixels(const LiveData &data, RideMood mood) {
  float speedKmh = data.speedCentimetersPerSecond == UNKNOWN_U16
                       ? 0.0f
                       : data.speedCentimetersPerSecond * 0.036f;
  float motion = 0.00018f + constrain(speedKmh, 0.0f, 50.0f) * 0.000014f;
  if (mood == MOOD_STOPPED) motion = 0.00012f;
  float zoneEnergy = data.heartRateZone == UNKNOWN_U8
                         ? 1.0f
                         : constrain((float)data.heartRateZone, 1.0f, 5.0f);
  float amplitude = 0.7f + zoneEnergy * 0.42f;
  float gradeTilt = constrain(data.gradeTenthsPercent / 50.0f, -1.8f, 1.8f);
  float time = millis() * motion;

  // Four very dense rows of permanent one-pixel dots. Nothing appears or vanishes:
  // speed controls flow, HR zone controls movement range and grade tilts the
  // field. This makes the strip meaningful while keeping it visually quiet.
  u8g2->setDrawColor(1);
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 74; ++column) {
      float noise = wireSmoothNoise(column * 0.072f, time + row * 0.31f);
      float tilt = gradeTilt * (column - 36) / 36.0f;
      int x = 3 + column * 4;
      int y = constrain((int)roundf(2 + row * 4 +
                                    (noise - 0.5f) * amplitude * 2.0f + tilt),
                        1, 16);
      u8g2->drawPixel(x, y);
      if (noise > 0.62f) {
        u8g2->drawPixel(x + 1, y);
        u8g2->drawPixel(x, y + 1);
        u8g2->drawPixel(x + 1, y + 1);
      }
      if (noise > 0.78f) u8g2->drawDisc(x, y, 1);
    }
  }
  u8g2->setDrawColor(1);
}

static bool wireSpeedAboveThirty = false;
static bool wirePushOverlayVisible = false;
static uint32_t wirePushOverlayStartedAt = 0;

static bool wireUpdatePushOverlay(const LiveData &data) {
  bool aboveThirty = data.timerState == 3 &&
                     data.speedCentimetersPerSecond != UNKNOWN_U16 &&
                     data.speedCentimetersPerSecond >= 834;  // 30.0 km/h.
  bool resetBelowThreshold = data.speedCentimetersPerSecond == UNKNOWN_U16 ||
                             data.speedCentimetersPerSecond < 806;  // 29 km/h.
  if (aboveThirty && !wireSpeedAboveThirty) {
    wireSpeedAboveThirty = true;
    wirePushOverlayStartedAt = millis();
  } else if (resetBelowThreshold) {
    wireSpeedAboveThirty = false;
  }
  wirePushOverlayVisible = wireSpeedAboveThirty &&
                           millis() - wirePushOverlayStartedAt < 10000UL;
  return wirePushOverlayVisible;
}

static bool wireAnimationActive() {
  return wirePushOverlayVisible;
}

static uint32_t wireAnimationIntervalMs() {
  if (wirePushOverlayVisible) return 80;
  if (liveData.connected && liveData.hasActivity) return 100;
  return 1000;
}

static void wireDrawPushOverlay(const LiveData &data) {
  uint32_t elapsed = millis() - wirePushOverlayStartedAt;
  // The entire panel becomes black. Layered noise particles, expanding
  // wavefronts and long acceleration streaks build depth around the focal disc.
  u8g2->setDrawColor(1);
  u8g2->drawBox(0, 0, LCD_WIDTH, LCD_HEIGHT);
  u8g2->setDrawColor(0);
  const int centerX = 150;
  const int centerY = 200;
  float pushTime = elapsed * 0.0032f;
  float travel = elapsed * 0.19f + elapsed * elapsed * 0.000007f;

  // Closely spaced wavefronts read as a smooth pulse rather than isolated rings.
  for (int wave = 0; wave < 7; ++wave) {
    int radius = (int)fmodf(travel * 0.58f + wave * 34.0f, 245.0f);
    if (radius > 24) {
      int driftX = (int)roundf(sinf(pushTime * 0.31f + wave) * 2.0f);
      int driftY = (int)roundf(cosf(pushTime * 0.27f + wave) * 3.0f);
      u8g2->drawCircle(centerX + driftX, centerY + driftY, radius);
    }
  }

  // Three particle depths move at different rates to create parallax.
  for (int layer = 0; layer < 3; ++layer) {
    int ringCount = layer == 0 ? 18 : (layer == 1 ? 13 : 9);
    int rayCount = layer == 0 ? 40 : (layer == 1 ? 32 : 24);
    for (int ring = 0; ring < ringCount; ++ring) {
      float spacing = layer == 0 ? 15.0f : (layer == 1 ? 22.0f : 31.0f);
      float velocity = layer == 0 ? 1.0f : (layer == 1 ? 0.76f : 0.54f);
      float radius = fmodf(ring * spacing + travel * velocity, 285.0f);
      for (int ray = 0; ray < rayCount; ++ray) {
        float noise = wireSmoothNoise(ray * 0.12f + layer * 8.0f,
                                      ring * 0.19f + pushTime);
        float threshold = layer == 0 ? 0.43f : (layer == 1 ? 0.51f : 0.58f);
        if (noise < threshold) continue;
        float angle = ray * (2.0f * PI / rayCount) +
                      (noise - 0.5f) * 0.31f +
                      pushTime * (layer == 0 ? 0.030f : (layer == 1 ? -0.022f : 0.014f));
        int x = centerX + (int)roundf(cosf(angle) * radius);
        int y = centerY + (int)roundf(sinf(angle) * radius);
        if (x < 2 || x >= 298 || y < 2 || y >= 398) continue;
        int size = constrain(1 + (int)(radius / 62.0f) +
                                 (noise > 0.80f ? 1 : 0),
                             1, 6);
        u8g2->drawBox(x - size / 2, y - size / 2, size, size);
        if (radius > 105.0f && noise > 0.68f) {
          float tailLength = 8.0f + radius * 0.055f;
          int tailX = centerX + (int)roundf(cosf(angle) * (radius - tailLength));
          int tailY = centerY + (int)roundf(sinf(angle) * (radius - tailLength));
          u8g2->drawLine(tailX, tailY, x, y);
        }
      }
    }
  }

  // Bright, sparse boost streaks sweep outward and give the final seconds a
  // stronger sense of acceleration without making the center harder to read.
  for (int ray = 0; ray < 44; ++ray) {
    float noise = wireSmoothNoise(ray * 0.21f, pushTime * 0.72f + 17.0f);
    if (noise < 0.60f) continue;
    float angle = ray * (2.0f * PI / 44.0f) + (noise - 0.5f) * 0.22f;
    float headRadius = 142.0f + fmodf(travel * (1.15f + noise * 0.55f) +
                                      ray * 37.0f, 155.0f);
    float streakLength = 12.0f + noise * 27.0f;
    int x1 = centerX + (int)roundf(cosf(angle) * (headRadius - streakLength));
    int y1 = centerY + (int)roundf(sinf(angle) * (headRadius - streakLength));
    int x2 = centerX + (int)roundf(cosf(angle) * headRadius);
    int y2 = centerY + (int)roundf(sinf(angle) * headRadius);
    u8g2->drawLine(x1, y1, x2, y2);
    if (noise > 0.82f) u8g2->drawLine(x1 + 1, y1, x2 + 1, y2);
  }

  // A large solid focal disc creates a strict label / speed / countdown
  // stack while reinforcing the expanding tunnel geometry around it.
  u8g2->setDrawColor(1);
  u8g2->drawDisc(centerX, centerY, 135);
  u8g2->setDrawColor(0);

  char value[16];
  u8g2->setFont(u8g2_font_helvB14_tf);
  const char *speedLabel = "SPEED KM/H";
  u8g2->drawStr((300 - u8g2->getStrWidth(speedLabel)) / 2, 124, speedLabel);

  u8g2->setFont(u8g2_font_inb63_mn);
  int overlayDecimalX = 0;
  wireDrawTightSpeed(data.speedCentimetersPerSecond * 0.036f,
                     222, overlayDecimalX, -1);

  uint8_t secondsLeft = 10 - min((uint32_t)9, elapsed / 1000UL);
  snprintf(value, sizeof(value), "%02u", secondsLeft);
  u8g2->setFont(u8g2_font_helvB18_tf);
  u8g2->drawStr((300 - u8g2->getStrWidth(value)) / 2, 286, value);
  u8g2->setDrawColor(1);
}

static void wireDrawCellLabel(int x, int baseline, const char *label) {
  u8g2->setFont(u8g2_font_helvB08_tf);
  u8g2->drawStr(x, baseline, label);
}

static uint32_t wireMapHash(uint32_t value) {
  value ^= value >> 16;
  value *= 0x7feb352dU;
  value ^= value >> 15;
  value *= 0x846ca68bU;
  return value ^ (value >> 16);
}

static void wireDrawTerrainTexture(const LiveData &data, int mapX, int mapY,
                                   int mapWidth, int mapHeight) {
  if (!data.hasGps) return;

  double centerLat = data.latitudeE7 / 10000000.0;
  double centerLon = data.longitudeE7 / 10000000.0;
  double widthMeters = 3000.0;
  double lonRange = widthMeters / (111320.0 * cos(centerLat * PI / 180.0));
  double latRange = widthMeters * mapHeight / mapWidth / 110540.0;
  double leftLon = centerLon - lonRange / 2.0;
  double bottomLat = centerLat - latRange / 2.0;

  for (int y = mapY + 6; y < mapY + mapHeight - 3; y += 7) {
    for (int x = mapX + 5; x < mapX + mapWidth - 3; x += 7) {
      double lon = leftLon + (x - mapX) * lonRange / mapWidth;
      double lat = bottomLat + (mapY + mapHeight - y) * latRange / mapHeight;
      int col = (int)floor((lon - ORM_GREEN_MIN_LON) / ORM_GREEN_CELL_LON);
      int row = (int)floor((lat - ORM_GREEN_MIN_LAT) / ORM_GREEN_CELL_LAT);
      if (col < 0 || col >= ORM_GREEN_COLS || row < 0 || row >= ORM_GREEN_ROWS)
        continue;
      uint32_t bit = (uint32_t)row * ORM_GREEN_COLS + col;
      if ((pgm_read_byte(ORM_GREEN_MASK + (bit >> 3)) & (1U << (bit & 7))) == 0)
        continue;

      uint32_t texture = wireMapHash(bit);
      u8g2->drawPixel(x, y);
      if ((texture & 15U) == 0U) {
        // Occasional tiny tree/vegetation mark within the stipple field.
        u8g2->drawPixel(x - 1, y);
        u8g2->drawPixel(x + 1, y);
        u8g2->drawPixel(x, y - 1);
      }
    }
  }
}

static void wireReadMapLabel(uint32_t offset, char *text, size_t capacity) {
  if (capacity == 0) return;
  size_t index = 0;
  while (index + 1 < capacity) {
    char character = (char)pgm_read_byte(ORM_LABEL_TEXT + offset + index);
    text[index++] = character;
    if (character == '\0') return;
  }
  text[capacity - 1] = '\0';
}

struct WireMapLabelCandidate {
  int x;
  int y;
  uint32_t text;
  uint32_t score;
  bool used;
};

static void wireDrawMapLabels(const LiveData &data, int mapX, int mapY,
                              int mapWidth, int mapHeight) {
  if (!data.hasGps) return;

  double centerLat = data.latitudeE7 / 10000000.0;
  double centerLon = data.longitudeE7 / 10000000.0;
  double widthMeters = 3000.0;
  double lonRange = widthMeters / (111320.0 * cos(centerLat * PI / 180.0));
  double latRange = widthMeters * mapHeight / mapWidth / 110540.0;
  double leftLon = centerLon - lonRange / 2.0;
  double bottomLat = centerLat - latRange / 2.0;
  double rightLon = leftLon + lonRange;
  double topLat = bottomLat + latRange;

  int firstCol = max(0, (int)floor((leftLon - ORM_MAP_MIN_LON) / ORM_MAP_TILE_LON));
  int lastCol = min((int)ORM_MAP_COLS - 1,
                    (int)floor((rightLon - ORM_MAP_MIN_LON) / ORM_MAP_TILE_LON));
  int firstRow = max(0, (int)floor((bottomLat - ORM_MAP_MIN_LAT) / ORM_MAP_TILE_LAT));
  int lastRow = min((int)ORM_MAP_ROWS - 1,
                    (int)floor((topLat - ORM_MAP_MIN_LAT) / ORM_MAP_TILE_LAT));

  WireMapLabelCandidate candidates[10];
  int candidateCount = 0;
  if (firstCol <= lastCol && firstRow <= lastRow) {
    for (int row = firstRow; row <= lastRow; ++row) {
      for (int col = firstCol; col <= lastCol; ++col) {
        OrmMapLabelTile tile;
        memcpy_P(&tile, &ORM_LABEL_INDEX[row * ORM_MAP_COLS + col], sizeof(tile));
        for (uint8_t item = 0; item < tile.count; ++item) {
          OrmMapLabel label;
          memcpy_P(&label, &ORM_LABELS[tile.start + item], sizeof(label));
          double lon = label.lonE7 / 10000000.0;
          double lat = label.latE7 / 10000000.0;
          int x = 0;
          int y = 0;
          if (!geoToMapScreen(lon, lat, leftLon, bottomLat, lonRange, latRange,
                              mapX, mapY, mapWidth, mapHeight, x, y)) continue;
          uint8_t rank = label.style & 0x0f;
          bool place = (label.style & 0x10) != 0;
          int dx = x - (mapX + mapWidth / 2);
          int dy = y - (mapY + mapHeight / 2);
          uint32_t score = rank * 10000U + (place ? 0U : 1200U) + dx * dx + dy * dy;
          WireMapLabelCandidate candidate = {x, y, label.text, score, false};
          if (candidateCount < 10) {
            candidates[candidateCount++] = candidate;
          } else {
            int worst = 0;
            for (int index = 1; index < 10; ++index)
              if (candidates[index].score > candidates[worst].score) worst = index;
            if (score < candidates[worst].score) candidates[worst] = candidate;
          }
        }
      }
    }
  }

  struct LabelBox { int left, top, right, bottom; } boxes[4];
  int boxCount = 0;
  u8g2->setFont(u8g2_font_helvB08_tf);
  for (int pass = 0; pass < candidateCount && boxCount < 4; ++pass) {
    int best = -1;
    for (int index = 0; index < candidateCount; ++index) {
      if (!candidates[index].used &&
          (best < 0 || candidates[index].score < candidates[best].score)) best = index;
    }
    if (best < 0) break;
    candidates[best].used = true;

    char text[23];
    wireReadMapLabel(candidates[best].text, text, sizeof(text));
    int width = min(mapWidth - 6, u8g2->getStrWidth(text) + 8);
    int left = constrain(candidates[best].x - width / 2, mapX + 2, mapX + mapWidth - width - 2);
    int top = constrain(candidates[best].y - 6, mapY + 7, mapY + mapHeight - 15);
    LabelBox box = {left, top, left + width, top + 13};
    bool collision = box.left < mapX + 60 && box.top < mapY + 15;
    if (box.left < mapX + mapWidth / 2 + 13 && box.right > mapX + mapWidth / 2 - 13 &&
        box.top < mapY + mapHeight / 2 + 13 && box.bottom > mapY + mapHeight / 2 - 13)
      collision = true;
    for (int index = 0; index < boxCount; ++index) {
      if (box.left <= boxes[index].right + 2 && box.right + 2 >= boxes[index].left &&
          box.top <= boxes[index].bottom + 1 && box.bottom + 1 >= boxes[index].top)
        collision = true;
    }
    if (collision) continue;
    u8g2->setDrawColor(0);
    u8g2->drawBox(box.left, box.top, width, 13);
    u8g2->setDrawColor(1);
    u8g2->drawStr(box.left + 4, box.top + 10, text);
    boxes[boxCount++] = box;
  }
}

static void wireDrawRichMap(const LiveData &data) {
  const int mapX = 0;
  const int mapY = 278;
  const int mapWidth = 300;
  const int mapHeight = 122;
  wireDrawTerrainTexture(data, mapX, mapY, mapWidth, mapHeight);
  drawMiniMap(data, mapX, mapY, mapWidth, mapHeight);
  wireDrawMapLabels(data, mapX, mapY, mapWidth, mapHeight);
  if (!data.hasGps) {
    u8g2->setDrawColor(0);
    u8g2->drawBox(5, mapY + 3, 52, 13);
    u8g2->setDrawColor(1);
    u8g2->setFont(u8g2_font_helvB10_tf);
    u8g2->drawStr(7, mapY + 13, "GPS MAP");
  }
}

static void wireDrawGpsTrack(const LiveData &data) {
  const int left = 10;
  const int right = 290;
  const int top = 338;
  const int bottom = 390;
  if (data.trackCount == 0) {
    u8g2->setFont(u8g2_font_helvB12_tf);
    u8g2->drawStr(10, 369, "WAITING FOR GPS");
    return;
  }

  uint16_t start = (data.trackHead + LiveData::TRACK_CAPACITY - data.trackCount) %
                   LiveData::TRACK_CAPACITY;
  int32_t minLat = INT32_MAX, maxLat = INT32_MIN;
  int32_t minLon = INT32_MAX, maxLon = INT32_MIN;
  for (uint16_t index = 0; index < data.trackCount; ++index) {
    uint16_t slot = (start + index) % LiveData::TRACK_CAPACITY;
    minLat = min(minLat, data.trackLatitudeE7[slot]);
    maxLat = max(maxLat, data.trackLatitudeE7[slot]);
    minLon = min(minLon, data.trackLongitudeE7[slot]);
    maxLon = max(maxLon, data.trackLongitudeE7[slot]);
  }
  int64_t latRange = max((int64_t)1, (int64_t)maxLat - minLat);
  int64_t lonRange = max((int64_t)1, (int64_t)maxLon - minLon);
  int previousX = left, previousY = bottom;
  for (uint16_t index = 0; index < data.trackCount; ++index) {
    uint16_t slot = (start + index) % LiveData::TRACK_CAPACITY;
    int x = left + (int)(((int64_t)data.trackLongitudeE7[slot] - minLon) *
                         (right - left) / lonRange);
    int y = bottom - (int)(((int64_t)data.trackLatitudeE7[slot] - minLat) *
                           (bottom - top) / latRange);
    if (index > 0) {
      u8g2->drawLine(previousX, previousY, x, y);
      u8g2->drawLine(previousX, previousY + 1, x, y + 1);
    }
    previousX = x;
    previousY = y;
  }
  u8g2->drawDisc(previousX, previousY, 4);
}

static void drawDashboard(const LiveData &data) {
  char value[48];
  RideMood mood = classifyMood(data);

  if (wireUpdatePushOverlay(data)) {
    wireDrawPushOverlay(data);
    return;
  }

  // Compact white generative strip: only black noise-driven dots remain.
  u8g2->setDrawColor(1);
  wireDrawMoodPixels(data, mood);
  u8g2->setDrawColor(1);
  u8g2->drawHLine(0, 18, 300);

  // The speed block is exactly two 52 px rows high. AVG and DIST each match
  // one of the small cards below; together they define the KM/H block height.
  u8g2->drawVLine(198, 18, 104);
  u8g2->drawHLine(198, 70, 102);
  u8g2->setFont(u8g2_font_helvB08_tf);
  wireDrawHeavy(10, 37, "SPEED KM/H");
  u8g2->setFont(u8g2_font_inb63_mn);
  int speedDecimalX = 155;
  if (data.speedCentimetersPerSecond == UNKNOWN_U16) {
    snprintf(value, sizeof(value), "--");
    u8g2->drawStr(10, 113, value);
  } else {
    wireDrawTightSpeed(data.speedCentimetersPerSecond * 0.036f, 113,
                       speedDecimalX, 10);
  }

  u8g2->setFont(u8g2_font_helvB08_tf);
  u8g2->drawStr(207, 36, "AVG SPEED");
  u8g2->drawStr(212, 88, "DIST KM");
  u8g2->setFont(u8g2_font_helvB14_tf);
  if (data.averageSpeedCentimetersPerSecond == UNKNOWN_U16)
    snprintf(value, sizeof(value), "--.-");
  else
    snprintf(value, sizeof(value), "%.1f", data.averageSpeedCentimetersPerSecond * 0.036f);
  u8g2->drawStr(212, 62, value);
  snprintf(value, sizeof(value), "%.1f", data.distanceDecimeters / 10000.0f);
  u8g2->drawStr(212, 114, value);
  u8g2->drawHLine(0, 122, 300);

  // Heart rate block.
  u8g2->drawVLine(99, 122, 52);
  u8g2->drawVLine(198, 122, 52);

  wireDrawHeart(14, 142);
  u8g2->setFont(u8g2_font_helvB14_tf);
  if (data.heartRate == UNKNOWN_U8) snprintf(value, sizeof(value), "--");
  else snprintf(value, sizeof(value), "%u", data.heartRate);
  u8g2->drawStr(37, 158, value);
  // Current HR and zone share the exact same font and baseline so they carry
  // equal visual weight and sit vertically centered in their cells.
  u8g2->setFont(u8g2_font_helvB14_tf);
  if (data.heartRateZone == UNKNOWN_U8) snprintf(value, sizeof(value), "ZONE -");
  else snprintf(value, sizeof(value), "ZONE %u", data.heartRateZone);
  u8g2->drawStr(112, 158, value);

  u8g2->setFont(u8g2_font_helvB08_tf);
  u8g2->drawStr(212, 139, "AVG HR");
  u8g2->setFont(u8g2_font_helvB14_tf);
  if (data.averageHeartRate == UNKNOWN_U8) snprintf(value, sizeof(value), "--");
  else snprintf(value, sizeof(value), "%u", data.averageHeartRate);
  u8g2->drawStr(212, 163, value);
  u8g2->drawHLine(0, 174, 300);

  // 2x3 stat grid.
  u8g2->drawVLine(99, 174, 104);
  u8g2->drawVLine(198, 174, 104);
  u8g2->drawHLine(0, 226, 300);
  u8g2->drawHLine(0, 278, 300);

  wireDrawCellLabel(14, 192, "TIME");
  wireDrawCellLabel(113, 192, "TEMP");
  wireDrawCellLabel(212, 192, "ASCENT");
  u8g2->setFont(u8g2_font_helvB14_tf);
  if (currentClockHour == UNKNOWN_U8) snprintf(value, sizeof(value), "--:--");
  else snprintf(value, sizeof(value), "%02u:%02u", currentClockHour, currentClockMinute);
  u8g2->drawStr(14, 217, value);
  if (isnan(ambientTemperatureC)) snprintf(value, sizeof(value), "-- C");
  else snprintf(value, sizeof(value), "%.0f C", ambientTemperatureC);
  u8g2->drawStr(113, 217, value);
  if (data.totalAscentDecimeters == UNKNOWN_U16) snprintf(value, sizeof(value), "-- M");
  else snprintf(value, sizeof(value), "%.0f M", data.totalAscentDecimeters / 10.0f);
  u8g2->drawStr(212, 217, value);

  wireDrawCellLabel(14, 244, "ELAPSED");
  wireDrawCellLabel(113, 244, "KCAL");
  wireDrawCellLabel(212, 244, "GRADE");
  formatDuration(data.timerSeconds, value, sizeof(value));
  u8g2->setFont(u8g2_font_helvB14_tf);
  u8g2->drawStr(14, 269, value);
  u8g2->setFont(u8g2_font_helvB14_tf);
  if (data.calories == UNKNOWN_U16) snprintf(value, sizeof(value), "--");
  else snprintf(value, sizeof(value), "%u", data.calories);
  u8g2->drawStr(113, 269, value);
  snprintf(value, sizeof(value), "%+.0f%%", data.gradeTenthsPercent / 10.0f);
  u8g2->drawStr(212, 269, value);

  // Offline Zagreb-region road map with the live GPS breadcrumb on top.
  wireDrawRichMap(data);
}

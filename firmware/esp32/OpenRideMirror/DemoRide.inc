// SPDX-License-Identifier: GPL-3.0-only
// Three real minutes, then the complete session resets and loops.
static const uint32_t DEMO_WALL_DURATION_MS = 180000UL;
static const uint32_t DEMO_RIDE_DURATION_SECONDS = 180UL;
static const uint32_t DEMO_UPDATE_INTERVAL_MS = 500UL;

struct DemoRoutePoint {
  double latitude;
  double longitude;
};

// A deliberately small closed sequence around eastern Jarun. It is long
// enough to demonstrate a moving pin and route line without racing across the
// complete Zagreb-region map during a short UI-development session.
static const DemoRoutePoint DEMO_ROUTE[] = {
    {45.78440, 15.90870},
    {45.78290, 15.91400},
    {45.77980, 15.91700},
    {45.77770, 15.91300},
    {45.77840, 15.90700},
    {45.78130, 15.90500},
    {45.78440, 15.90870},
};
static const size_t DEMO_ROUTE_COUNT = sizeof(DEMO_ROUTE) / sizeof(DEMO_ROUTE[0]);

static uint32_t demoStartedAt = 0;
static uint32_t demoLastUpdateAt = 0;
static double demoPreviousRideSeconds = 0.0;
static double demoDistanceDecimeters = 0.0;
static double demoAltitudeDecimeters = 1180.0;
static double demoAscentDecimeters = 0.0;
static double demoHeartRateTotal = 0.0;
static uint32_t demoHeartRateSamples = 0;
static uint8_t demoMaximumHeartRate = 0;
static uint16_t demoMaximumSpeed = 0;

static float demoSpeedForSecond(double second) {
  if (second < 20.0) return 10.0f + (float)(second / 20.0) * 14.0f;
  if (second < 45.0) return 25.0f + sinf((float)second * 0.25f) * 1.5f;
  if (second < 70.0) return 34.0f + sinf((float)second * 0.35f) * 2.0f;
  if (second < 90.0) return 17.0f + sinf((float)second * 0.20f);
  if (second < 115.0) return 38.0f + (float)(second - 90.0) / 25.0f * 4.0f;
  if (second < 140.0) return 26.0f + sinf((float)second * 0.18f) * 2.0f;
  if (second < 158.0) return 33.0f + sinf((float)second * 0.30f) * 2.0f;
  if (second < 165.0) return 21.0f;
  return max(0.0f, 21.0f - (float)(second - 165.0) / 15.0f * 21.0f);
}

static float demoGradeForSecond(double second) {
  if (second < 20.0) return 0.5f;
  if (second < 45.0) return sinf((float)second * 0.18f) * 1.0f;
  if (second < 70.0) return 1.8f;
  if (second < 90.0) return 6.5f + sinf((float)second * 0.24f);
  if (second < 115.0) return -5.5f;
  if (second < 140.0) return sinf((float)second * 0.16f) * 1.3f;
  if (second < 158.0) return 2.8f;
  return -0.5f;
}

static void demoRoutePosition(double progress, double &latitude, double &longitude,
                              uint16_t &headingCentidegrees) {
  progress = constrain(progress, 0.0, 0.999999);
  double routePosition = progress * (DEMO_ROUTE_COUNT - 1);
  size_t segment = min((size_t)floor(routePosition), DEMO_ROUTE_COUNT - 2);
  double local = routePosition - segment;
  const DemoRoutePoint &from = DEMO_ROUTE[segment];
  const DemoRoutePoint &to = DEMO_ROUTE[segment + 1];
  latitude = from.latitude + (to.latitude - from.latitude) * local;
  longitude = from.longitude + (to.longitude - from.longitude) * local;

  double north = to.latitude - from.latitude;
  double east = (to.longitude - from.longitude) * cos(latitude * PI / 180.0);
  double heading = atan2(east, north) * 180.0 / PI;
  if (heading < 0.0) heading += 360.0;
  headingCentidegrees = (uint16_t)round(heading * 100.0);
}

static void demoFillPlannedRoute() {
  liveData.trackHead = 0;
  liveData.trackCount = 0;
  const uint16_t samples = 150;
  for (uint16_t sample = 0; sample < samples; ++sample) {
    double latitude, longitude;
    uint16_t heading;
    demoRoutePosition(sample / (double)(samples - 1), latitude, longitude, heading);
    liveData.trackLatitudeE7[liveData.trackHead] = (int32_t)llround(latitude * 10000000.0);
    liveData.trackLongitudeE7[liveData.trackHead] = (int32_t)llround(longitude * 10000000.0);
    liveData.trackHead = (liveData.trackHead + 1) % LiveData::TRACK_CAPACITY;
    liveData.trackCount++;
  }
}

static void demoReset(uint32_t now) {
  demoStartedAt = now;
  demoLastUpdateAt = 0;
  demoPreviousRideSeconds = 0.0;
  demoDistanceDecimeters = 0.0;
  demoAltitudeDecimeters = 1180.0;
  demoAscentDecimeters = 0.0;
  demoHeartRateTotal = 0.0;
  demoHeartRateSamples = 0;
  demoMaximumHeartRate = 0;
  demoMaximumSpeed = 0;

  portENTER_CRITICAL(&dataMux);
  liveData = LiveData();
  liveData.connected = true;
  liveData.hasActivity = true;
  liveData.hasGps = true;
  liveData.sport = 2;  // Cycling.
  liveData.subSport = UNKNOWN_U8;
  liveData.gpsQuality = 4;
  liveData.timerState = 3;
  liveData.lastPacketAt = now;
  liveData.lastGpsAt = now;
  demoFillPlannedRoute();
  portEXIT_CRITICAL(&dataMux);
  screenDirty = true;
  Serial.println("ESP demo ride: new 3-minute Jarun loop");
}

static void demoRideBegin() {
  demoReset(millis());
}

static void demoRideUpdate() {
  uint32_t now = millis();
  if (demoLastUpdateAt != 0 && now - demoLastUpdateAt < DEMO_UPDATE_INTERVAL_MS) return;
  demoLastUpdateAt = now;

  uint32_t wallElapsed = now - demoStartedAt;
  if (wallElapsed >= DEMO_WALL_DURATION_MS) {
    demoReset(now);
    wallElapsed = 0;
  }

  double progress = wallElapsed / (double)DEMO_WALL_DURATION_MS;
  double rideSeconds = progress * DEMO_RIDE_DURATION_SECONDS;
  double deltaSeconds = max(0.0, rideSeconds - demoPreviousRideSeconds);
  demoPreviousRideSeconds = rideSeconds;

  float speedKmh = demoSpeedForSecond(rideSeconds);
  float gradePercent = demoGradeForSecond(rideSeconds);
  uint16_t speedCms = (uint16_t)round(speedKmh / 0.036f);
  double distanceDeltaDecimeters = speedKmh / 3.6 * deltaSeconds * 10.0;
  demoDistanceDecimeters += distanceDeltaDecimeters;
  double altitudeDelta = distanceDeltaDecimeters * gradePercent / 100.0;
  demoAltitudeDecimeters += altitudeDelta;
  if (altitudeDelta > 0.0) demoAscentDecimeters += altitudeDelta;
  demoMaximumSpeed = max(demoMaximumSpeed, speedCms);

  float heartRateFloat = speedKmh < 1.0f
                             ? 92.0f
                             : 101.0f + speedKmh * 1.65f + max(0.0f, gradePercent) * 4.0f +
                                   sinf((float)rideSeconds * 0.09f) * 4.0f;
  uint8_t heartRate = (uint8_t)constrain((int)round(heartRateFloat), 88, 184);
  uint8_t zone = heartRate < 115 ? 1 : heartRate < 135 ? 2 : heartRate < 152 ? 3
                                                            : heartRate < 168   ? 4
                                                                                : 5;
  demoHeartRateTotal += heartRate;
  demoHeartRateSamples++;
  demoMaximumHeartRate = max(demoMaximumHeartRate, heartRate);

  double latitude, longitude;
  uint16_t heading;
  demoRoutePosition(progress, latitude, longitude, heading);

  uint32_t elapsedWhole = (uint32_t)rideSeconds;
  uint16_t averageSpeed = elapsedWhole == 0
                              ? speedCms
                              : (uint16_t)round(demoDistanceDecimeters * 10.0 / elapsedWhole);
  uint16_t calories = (uint16_t)round(rideSeconds / 60.0 * (5.5 + speedKmh * 0.13));

  portENTER_CRITICAL(&dataMux);
  liveData.connected = true;
  liveData.hasActivity = true;
  liveData.hasGps = true;
  liveData.sequence++;
  liveData.timerState = rideSeconds >= 178.0 ? 1 : 3;
  liveData.timerSeconds = elapsedWhole;
  liveData.gpsTimerSeconds = elapsedWhole;
  liveData.distanceDecimeters = (uint32_t)round(demoDistanceDecimeters);
  liveData.speedCentimetersPerSecond = speedCms;
  liveData.averageSpeedCentimetersPerSecond = averageSpeed;
  liveData.maxSpeedCentimetersPerSecond = demoMaximumSpeed;
  liveData.heartRate = heartRate;
  liveData.heartRateZone = zone;
  liveData.averageHeartRate = demoHeartRateSamples == 0
                                  ? heartRate
                                  : (uint8_t)round(demoHeartRateTotal / demoHeartRateSamples);
  liveData.maxHeartRate = demoMaximumHeartRate;
  liveData.calories = calories;
  liveData.totalAscentDecimeters = (uint16_t)round(demoAscentDecimeters);
  liveData.gradeTenthsPercent = (int16_t)round(gradePercent * 10.0f);
  liveData.altitudeDecimeters = (int16_t)round(demoAltitudeDecimeters);
  liveData.latitudeE7 = (int32_t)llround(latitude * 10000000.0);
  liveData.longitudeE7 = (int32_t)llround(longitude * 10000000.0);
  liveData.headingCentidegrees = heading;
  liveData.gpsQuality = 4;
  liveData.lastPacketAt = now;
  liveData.lastGpsAt = now;
  liveData.packetCount++;
  portEXIT_CRITICAL(&dataMux);

  screenDirty = true;
}

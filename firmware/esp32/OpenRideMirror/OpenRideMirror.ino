// SPDX-License-Identifier: GPL-3.0-only
// OpenRideMirror firmware for Waveshare ESP32-S3-RLCD-4.2.
#define ORM_WIREFRAME_LAYOUT 1
#ifndef ORM_BUILD_DEMO
#define ORM_BUILD_DEMO 0
#endif

#include "ST7305_U8g2.h"
#include "OrmMapData.h"
#include "OrmMapLabels.h"
#include "OrmGreenMask.h"
#include "OrmBoard.h"

#include <BLE2902.h>
#include <BLEAdvertising.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEService.h>
#include <U8g2lib.h>
#include <Preferences.h>

#include "OrmPowerState.h"

#ifndef ORM_WIREFRAME_LAYOUT
#define ORM_WIREFRAME_LAYOUT 1
#endif

#define LCD_WIDTH 300
#define LCD_HEIGHT 400
#define RLCD_SCK_PIN 11
#define RLCD_MOSI_PIN 12
#define RLCD_DC_PIN 5
#define RLCD_CS_PIN 40
#define RLCD_RST_PIN 41

static const char *DEVICE_NAME = "ORM";
static const char *SERVICE_UUID = "D8185099-1302-4FEB-906F-0AE8D5329ABA";
static const char *TELEMETRY_UUID = "734A1ED9-8E4D-4AEB-A5D7-BEABC20643B8";

static const uint8_t PACKET_ACTIVITY = 0x10;
static const uint8_t PACKET_GPS = 0x11;
static const uint8_t PACKET_EXTENDED = 0x12;
static const uint8_t PROTOCOL_VERSION = 1;
// ORM protocol v1 sub-sport codes; see development/protocol/orm-protocol.json.
static const uint8_t SUB_SPORT_ROAD = 1;
static const uint8_t SUB_SPORT_INDOOR_CYCLING = 2;
static const uint8_t SUB_SPORT_SPIN = 3;
static const uint8_t SUB_SPORT_VIRTUAL_ACTIVITY = 4;

static const uint8_t UNKNOWN_U8 = 0xff;
static const uint16_t UNKNOWN_U16 = 0xffff;
static const int16_t UNKNOWN_S16 = INT16_MIN;

struct LiveData {
  bool connected = false;
  bool hasActivity = false;
  bool hasGps = false;
  uint8_t sequence = 0;
  uint8_t timerState = 0;
  uint8_t sport = UNKNOWN_U8;
  uint8_t subSport = UNKNOWN_U8;
  uint8_t heartRate = UNKNOWN_U8;
  uint8_t cadenceRpm = UNKNOWN_U8;
  uint16_t powerWatts = UNKNOWN_U16;
  uint8_t powerZone = UNKNOWN_U8;
  uint32_t timerSeconds = 0;
  uint32_t distanceDecimeters = 0;
  uint16_t speedCentimetersPerSecond = UNKNOWN_U16;
  uint8_t gpsQuality = UNKNOWN_U8;
  int32_t latitudeE7 = 0;
  int32_t longitudeE7 = 0;
  int16_t altitudeDecimeters = UNKNOWN_S16;
  uint16_t headingCentidegrees = UNKNOWN_U16;
  uint32_t gpsTimerSeconds = 0;
  uint32_t packetCount = 0;
  uint32_t invalidPacketCount = 0;
  uint32_t lastPacketAt = 0;
  uint32_t lastGpsAt = 0;
  uint8_t heartRateZone = UNKNOWN_U8;
  uint8_t averageHeartRate = UNKNOWN_U8;
  uint8_t maxHeartRate = UNKNOWN_U8;
  uint16_t averageSpeedCentimetersPerSecond = UNKNOWN_U16;
  uint16_t maxSpeedCentimetersPerSecond = UNKNOWN_U16;
  uint16_t totalAscentDecimeters = UNKNOWN_U16;
  uint16_t calories = UNKNOWN_U16;
  uint8_t clockHour = UNKNOWN_U8;
  uint8_t clockMinute = UNKNOWN_U8;
  uint8_t clockSecond = UNKNOWN_U8;
  int16_t gradeTenthsPercent = 0;
  static const uint16_t TRACK_CAPACITY = 160;
  int32_t trackLatitudeE7[TRACK_CAPACITY] = {};
  int32_t trackLongitudeE7[TRACK_CAPACITY] = {};
  uint16_t trackHead = 0;
  uint16_t trackCount = 0;
};

enum RideMood : uint8_t {
  MOOD_STOPPED,
  MOOD_CRUISING,
  MOOD_PUSHING,
  MOOD_CLIMBING,
  MOOD_DESCENDING,
  MOOD_LOST,
};

struct __attribute__((packed)) RideSummary {
  uint8_t sport = UNKNOWN_U8;
  uint8_t startHour = UNKNOWN_U8;
  uint8_t startMinute = UNKNOWN_U8;
  uint8_t averageHeartRate = UNKNOWN_U8;
  uint8_t maxHeartRate = UNKNOWN_U8;
  uint32_t durationSeconds = 0;
  uint32_t distanceDecimeters = 0;
  uint16_t averageSpeedCentimetersPerSecond = UNKNOWN_U16;
  uint16_t maxSpeedCentimetersPerSecond = UNKNOWN_U16;
  uint16_t ascentDecimeters = UNKNOWN_U16;
  uint16_t calories = UNKNOWN_U16;
};

struct __attribute__((packed)) RideHistory {
  uint32_t magic = 0x4f524d48;  // ORMH
  uint8_t version = 1;
  uint8_t count = 0;
  uint16_t reserved = 0;
  RideSummary rides[5] = {};
  uint32_t checksum = 0;
};

static ST7305_U8g2 lcd(RLCD_SCK_PIN, RLCD_MOSI_PIN, RLCD_DC_PIN, RLCD_CS_PIN, RLCD_RST_PIN);
static U8G2 *u8g2 = nullptr;
static BLEServer *bleServer = nullptr;
static LiveData liveData;
static portMUX_TYPE dataMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool screenDirty = true;

// Ultimo movimiento reportado por el acelerometro. Cero significa "todavia no
// se vio moverse la bici desde el arranque", que decidePowerState trata como
// motivo para quedarse despierto: una placa encendida en el garaje no debe
// dormirse antes de haber medido nada.
//
// El driver del MPU6050 todavia no existe (la placa y el sensor no llegaron),
// asi que por ahora esto solo lo actualiza la telemetria del reloj. La decision
// de dormir ya esta escrita y probada; lo que falta es quien la alimente.
static volatile uint32_t lastMotionAtMs = 0;

// El reloj esta conectado y con el cronometro corriendo. Mientras eso valga, el
// ciclista esta en plena salida y la pantalla se queda viva por quieta que este
// la bici -- pararse en un semaforo es justo cuando uno mira los datos.
static bool rideInProgress(const LiveData &data) {
  return data.connected && data.timerState == 3;
}

static orm::PowerState currentPowerState(const LiveData &data, uint32_t nowMs) {
  orm::PowerInputs inputs;
  inputs.nowMs = nowMs;
  inputs.lastMotionMs = lastMotionAtMs;
  inputs.rideInProgress = rideInProgress(data);
  return orm::decidePowerState(inputs);
}
static uint32_t lastDrawAt = 0;
static orm::I2cBus boardI2c(14, 13, 0);
static orm::Board board;
static Preferences historyPreferences;
static RideHistory rideHistory;
static bool rideSessionActive = false;
static bool historySavePending = false;
static uint8_t previousTimerState = UNKNOWN_U8;
static uint16_t nextAnnouncementKm = 5;
static uint32_t lastGpsDistanceDecimeters = 0;
static int16_t lastGradeAltitudeDecimeters = UNKNOWN_S16;
static float ambientTemperatureC = NAN;
static float ambientHumidityPercent = NAN;
static uint32_t lastEnvironmentReadAt = 0;
static volatile bool clockSyncPending = false;
static uint8_t pendingClockHour = 0;
static uint8_t pendingClockMinute = 0;
static uint8_t pendingClockSecond = 0;
static uint8_t currentClockHour = UNKNOWN_U8;
static uint8_t currentClockMinute = UNKNOWN_U8;
static uint8_t currentClockSecond = UNKNOWN_U8;
static uint32_t lastClockReadAt = 0;

static uint16_t readU16(const uint8_t *data, size_t offset) {
  return (uint16_t)data[offset] | ((uint16_t)data[offset + 1] << 8);
}

static int16_t readS16(const uint8_t *data, size_t offset) {
  return (int16_t)readU16(data, offset);
}

static uint32_t readU32(const uint8_t *data, size_t offset) {
  return (uint32_t)data[offset] |
         ((uint32_t)data[offset + 1] << 8) |
         ((uint32_t)data[offset + 2] << 16) |
         ((uint32_t)data[offset + 3] << 24);
}

static int32_t readS32(const uint8_t *data, size_t offset) {
  return (int32_t)readU32(data, offset);
}

static bool validTelemetryPacket(const uint8_t *bytes, size_t length) {
  if (length != 20 || bytes[1] != PROTOCOL_VERSION) return false;
  if (bytes[0] == PACKET_ACTIVITY) {
    if (bytes[3] > 3) return false;
    const uint16_t speed = readU16(bytes, 16);
    return speed == UNKNOWN_U16 || speed <= 6000;  // 216 km/h sanity limit.
  }
  if (bytes[0] == PACKET_GPS) {
    const int32_t latitude = readS32(bytes, 4);
    const int32_t longitude = readS32(bytes, 8);
    const uint16_t heading = readU16(bytes, 14);
    return latitude >= -900000000 && latitude <= 900000000 &&
           longitude >= -1800000000 && longitude <= 1800000000 &&
           (heading == UNKNOWN_U16 || heading < 36000);
  }
  if (bytes[0] == PACKET_EXTENDED) {
    const uint8_t zone = bytes[3];
    const bool validZone = zone == UNKNOWN_U8 || zone <= 5;
    const bool validClock =
        (bytes[14] == UNKNOWN_U8 && bytes[15] == UNKNOWN_U8 && bytes[16] == UNKNOWN_U8) ||
        (bytes[14] < 24 && bytes[15] < 60 && bytes[16] < 60);
    return validZone && validClock;
  }
  return false;
}

static const char *sportName(uint8_t sport) {
  switch (sport) {
    case 0: return "ACTIVITY";
    case 1: return "RUNNING";
    case 2: return "CYCLING";
    case 4: return "FITNESS";
    case 5: return "SWIMMING";
    case 10: return "TRAINING";
    case 11: return "WALKING";
    case 12: return "XC SKI";
    case 13: return "SKIING";
    case 14: return "SNOWBOARD";
    case 15: return "ROWING";
    case 16: return "MOUNTAINEER";
    case 17: return "HIKING";
    case 18: return "MULTISPORT";
    case 19: return "PADDLING";
    case 21: return "E-BIKING";
    case 25: return "GOLF";
    case 31: return "CLIMBING";
    case 35: return "SNOWSHOE";
    case 37: return "SUP";
    case 38: return "SURFING";
    case 41: return "KAYAKING";
    case 47: return "BOXING";
    case 62: return "HIIT";
    default: return "GARMIN LIVE";
  }
}

static const char *timerStateName(uint8_t state) {
  switch (state) {
    case 0: return "READY";
    case 1: return "STOPPED";
    case 2: return "AUTO PAUSE";
    case 3: return "LIVE";
    default: return "UNKNOWN";
  }
}

static const char *moodName(RideMood mood) {
  switch (mood) {
    case MOOD_CRUISING: return "CRUISING";
    case MOOD_PUSHING: return "PUSHING";
    case MOOD_CLIMBING: return "CLIMBING";
    case MOOD_DESCENDING: return "DESCENDING";
    case MOOD_LOST: return "LOST?";
    default: return "STOPPED";
  }
}

static RideMood classifyMood(const LiveData &data) {
  if (data.connected && data.lastPacketAt && millis() - data.lastPacketAt > 6500) return MOOD_LOST;
  if (data.timerState != 3 || data.speedCentimetersPerSecond == UNKNOWN_U16 ||
      data.speedCentimetersPerSecond < 80) return MOOD_STOPPED;
  if (data.gradeTenthsPercent >= 25) return MOOD_CLIMBING;
  if (data.gradeTenthsPercent <= -25) return MOOD_DESCENDING;
  if (data.heartRateZone >= 4 && data.heartRateZone <= 5) return MOOD_PUSHING;
  return MOOD_CRUISING;
}

static uint32_t historyChecksum(const RideHistory &history) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&history);
  const size_t length = offsetof(RideHistory, checksum);
  uint32_t hash = 2166136261u;
  for (size_t index = 0; index < length; ++index) {
    hash ^= bytes[index];
    hash *= 16777619u;
  }
  return hash;
}

static void loadRideHistory() {
  historyPreferences.begin("ride_history", false);
  size_t stored = historyPreferences.getBytesLength("rides");
  if (stored == sizeof(rideHistory)) {
    historyPreferences.getBytes("rides", &rideHistory, sizeof(rideHistory));
  }
  if (rideHistory.magic != 0x4f524d48 || rideHistory.version != 1 ||
      rideHistory.count > 5 || rideHistory.checksum != historyChecksum(rideHistory)) {
    rideHistory = RideHistory();
  }
}

static void saveRideHistory() {
  rideHistory.checksum = historyChecksum(rideHistory);
  historyPreferences.putBytes("rides", &rideHistory, sizeof(rideHistory));
  historySavePending = false;
}

static void beginRideSummary() {
  for (int index = 4; index > 0; --index) rideHistory.rides[index] = rideHistory.rides[index - 1];
  rideHistory.rides[0] = RideSummary();
  rideHistory.count = min((int)rideHistory.count + 1, 5);
  portENTER_CRITICAL(&dataMux);
  liveData.trackHead = 0;
  liveData.trackCount = 0;
  liveData.gradeTenthsPercent = 0;
  portEXIT_CRITICAL(&dataMux);
  lastGpsDistanceDecimeters = 0;
  lastGradeAltitudeDecimeters = UNKNOWN_S16;
  rideSessionActive = true;
  nextAnnouncementKm = 5;
}

static void updateRideSummary(const LiveData &data) {
  if (!rideSessionActive || rideHistory.count == 0) return;
  RideSummary &ride = rideHistory.rides[0];
  ride.sport = data.sport;
  if (ride.startHour == UNKNOWN_U8 && data.clockHour != UNKNOWN_U8) {
    ride.startHour = data.clockHour;
    ride.startMinute = data.clockMinute;
  }
  ride.durationSeconds = data.timerSeconds;
  ride.distanceDecimeters = data.distanceDecimeters;
  ride.averageHeartRate = data.averageHeartRate;
  ride.maxHeartRate = data.maxHeartRate;
  ride.averageSpeedCentimetersPerSecond = data.averageSpeedCentimetersPerSecond;
  ride.maxSpeedCentimetersPerSecond = data.maxSpeedCentimetersPerSecond;
  ride.ascentDecimeters = data.totalAscentDecimeters;
  ride.calories = data.calories;
}

static void handleTimerTransition(uint8_t nextState) {
  if (nextState == previousTimerState) return;
  if (nextState == 3) {
    if (!rideSessionActive) {
      beginRideSummary();
    } else {
    }
  } else if (previousTimerState == 3 && nextState == 2) {
    historySavePending = true;
  } else if (previousTimerState == 3 && (nextState == 0 || nextState == 1)) {
    historySavePending = true;
  }
  previousTimerState = nextState;
}

static void markInvalidPacket() {
  portENTER_CRITICAL(&dataMux);
  liveData.invalidPacketCount++;
  portEXIT_CRITICAL(&dataMux);
  screenDirty = true;
}

static void parseTelemetry(const uint8_t *bytes, size_t length) {
  if (!validTelemetryPacket(bytes, length)) {
    markInvalidPacket();
    Serial.printf("Rejected BLE packet: length=%u type=%u version=%u\n", (unsigned)length,
                  length > 0 ? bytes[0] : 0, length > 1 ? bytes[1] : 0);
    return;
  }

  bool activityPacket = false;
  bool shouldAnnounceStats = false;
  uint8_t nextTimerState = previousTimerState;
  portENTER_CRITICAL(&dataMux);
  if (bytes[0] == PACKET_ACTIVITY) {
    activityPacket = true;
    liveData.sequence = bytes[2];
    liveData.timerState = bytes[3];
    nextTimerState = liveData.timerState;
    liveData.sport = bytes[4];
    liveData.subSport = bytes[5];
    liveData.heartRate = bytes[6];
    liveData.cadenceRpm = bytes[7];
    liveData.timerSeconds = readU32(bytes, 8);
    liveData.distanceDecimeters = readU32(bytes, 12);
    liveData.speedCentimetersPerSecond = readU16(bytes, 16);
    liveData.powerWatts = readU16(bytes, 18);
    // Fuente de movimiento provisional hasta que exista el MPU6050: si el reloj
    // reporta velocidad real, la bici se esta moviendo. No reemplaza al
    // acelerometro (no ve nada con el reloj desconectado), pero evita que la
    // logica quede sin alimentar mientras tanto.
    if (liveData.speedCentimetersPerSecond != UNKNOWN_U16 &&
        liveData.speedCentimetersPerSecond > 0) {
      lastMotionAtMs = millis();
    }
    liveData.hasActivity = true;
    uint16_t wholeKilometers = liveData.distanceDecimeters / 10000;
    if (liveData.timerState == 3 && wholeKilometers >= nextAnnouncementKm) {
      shouldAnnounceStats = true;
      while (nextAnnouncementKm <= wholeKilometers) nextAnnouncementKm += 5;
    }
  } else if (bytes[0] == PACKET_GPS) {
    liveData.sequence = bytes[2];
    liveData.gpsQuality = bytes[3];
    liveData.latitudeE7 = readS32(bytes, 4);
    liveData.longitudeE7 = readS32(bytes, 8);
    liveData.altitudeDecimeters = readS16(bytes, 12);
    liveData.headingCentidegrees = readU16(bytes, 14);
    liveData.gpsTimerSeconds = readU32(bytes, 16);
    liveData.hasGps = true;
    liveData.lastGpsAt = millis();
    if (lastGradeAltitudeDecimeters != UNKNOWN_S16 &&
        liveData.distanceDecimeters > lastGpsDistanceDecimeters + 150) {
      int32_t altitudeDelta = liveData.altitudeDecimeters - lastGradeAltitudeDecimeters;
      uint32_t distanceDelta = liveData.distanceDecimeters - lastGpsDistanceDecimeters;
      int32_t rawGradeTenths = constrain((int32_t)(altitudeDelta * 1000 / (int32_t)distanceDelta), -250, 250);
      liveData.gradeTenthsPercent = (int16_t)((liveData.gradeTenthsPercent * 3 + rawGradeTenths) / 4);
    }
    if (liveData.altitudeDecimeters != UNKNOWN_S16) {
      lastGradeAltitudeDecimeters = liveData.altitudeDecimeters;
      lastGpsDistanceDecimeters = liveData.distanceDecimeters;
    }
    bool appendTrack = liveData.trackCount == 0;
    if (!appendTrack) {
      uint16_t previous = (liveData.trackHead + LiveData::TRACK_CAPACITY - 1) % LiveData::TRACK_CAPACITY;
      int32_t latitudeDelta = abs(liveData.latitudeE7 - liveData.trackLatitudeE7[previous]);
      int32_t longitudeDelta = abs(liveData.longitudeE7 - liveData.trackLongitudeE7[previous]);
      appendTrack = latitudeDelta + longitudeDelta >= 500;  // Roughly five metres.
    }
    if (appendTrack) {
      liveData.trackLatitudeE7[liveData.trackHead] = liveData.latitudeE7;
      liveData.trackLongitudeE7[liveData.trackHead] = liveData.longitudeE7;
      liveData.trackHead = (liveData.trackHead + 1) % LiveData::TRACK_CAPACITY;
      if (liveData.trackCount < LiveData::TRACK_CAPACITY) liveData.trackCount++;
    }
  } else if (bytes[0] == PACKET_EXTENDED) {
    liveData.sequence = bytes[2];
    liveData.heartRateZone = bytes[3];
    liveData.averageHeartRate = bytes[4];
    liveData.maxHeartRate = bytes[5];
    liveData.averageSpeedCentimetersPerSecond = readU16(bytes, 6);
    liveData.maxSpeedCentimetersPerSecond = readU16(bytes, 8);
    liveData.totalAscentDecimeters = readU16(bytes, 10);
    liveData.calories = readU16(bytes, 12);
    liveData.clockHour = bytes[14];
    liveData.clockMinute = bytes[15];
    liveData.clockSecond = bytes[16];
    liveData.powerZone = bytes[17];
    // Bytes 18..19 are reserved in ORM Protocol v1.
    if (liveData.clockHour < 24 && liveData.clockMinute < 60 && liveData.clockSecond < 60) {
      pendingClockHour = liveData.clockHour;
      pendingClockMinute = liveData.clockMinute;
      pendingClockSecond = liveData.clockSecond;
      clockSyncPending = true;
    }
  } else {
    liveData.invalidPacketCount++;
    portEXIT_CRITICAL(&dataMux);
    screenDirty = true;
    Serial.printf("Rejected BLE packet type: 0x%02x\n", bytes[0]);
    return;
  }
  liveData.packetCount++;
  liveData.lastPacketAt = millis();
  portEXIT_CRITICAL(&dataMux);

  if (activityPacket) handleTimerTransition(nextTimerState);
  updateRideSummary(liveData);
  if (bytes[0] == PACKET_EXTENDED && rideSessionActive && liveData.timerState != 3)
    historySavePending = true;
  if (shouldAnnounceStats) historySavePending = true;

  screenDirty = true;
  Serial.printf("BLE packet type=0x%02x seq=%u\n", bytes[0], bytes[2]);
}

class TelemetryCallbacks : public BLECharacteristicCallbacks {
 public:
  void onWrite(BLECharacteristic *characteristic) override {
    String value = characteristic->getValue();
    parseTelemetry(reinterpret_cast<const uint8_t *>(value.c_str()), value.length());
  }
};

class ServerCallbacks : public BLEServerCallbacks {
 public:
  void onConnect(BLEServer *server) override {
    (void)server;
    portENTER_CRITICAL(&dataMux);
    liveData.connected = true;
    portEXIT_CRITICAL(&dataMux);
    screenDirty = true;
    Serial.println("Garmin BLE client connected");
  }

  void onDisconnect(BLEServer *server) override {
    (void)server;
    portENTER_CRITICAL(&dataMux);
    bool rideWasStopped = liveData.timerState != 3;
    liveData.connected = false;
    portEXIT_CRITICAL(&dataMux);
    screenDirty = true;
    if (rideSessionActive) historySavePending = true;
    if (rideWasStopped) rideSessionActive = false;
    Serial.println("Garmin BLE client disconnected; advertising restarted");
  }
};

static void drawTextRight(int x, int y, const char *text) {
  u8g2->drawStr(x - u8g2->getStrWidth(text), y, text);
}

static void drawTextCentered(int centerX, int y, const char *text) {
  u8g2->drawStr(centerX - u8g2->getStrWidth(text) / 2, y, text);
}

static void drawHeartIcon(int x, int y) {
  u8g2->drawDisc(x + 4, y + 4, 4);
  u8g2->drawDisc(x + 10, y + 4, 4);
  u8g2->drawTriangle(x, y + 5, x + 14, y + 5, x + 7, y + 15);
}

static void drawSpeedIcon(int x, int y) {
  u8g2->drawCircle(x + 7, y + 7, 6);
  u8g2->drawLine(x + 7, y + 7, x + 11, y + 3);
  u8g2->drawDisc(x + 7, y + 7, 1);
}

static void drawPinIcon(int x, int y) {
  u8g2->drawCircle(x + 6, y + 5, 5);
  u8g2->drawDisc(x + 6, y + 5, 1);
  u8g2->drawLine(x + 3, y + 9, x + 6, y + 15);
  u8g2->drawLine(x + 9, y + 9, x + 6, y + 15);
}

static void drawClockIcon(int x, int y) {
  u8g2->drawCircle(x + 7, y + 7, 6);
  u8g2->drawLine(x + 7, y + 7, x + 7, y + 3);
  u8g2->drawLine(x + 7, y + 7, x + 10, y + 9);
}

static void formatDuration(uint32_t seconds, char *buffer, size_t size) {
  uint32_t hours = seconds / 3600;
  uint32_t minutes = (seconds / 60) % 60;
  uint32_t secs = seconds % 60;
  snprintf(buffer, size, "%02lu:%02lu:%02lu", (unsigned long)hours,
           (unsigned long)minutes, (unsigned long)secs);
}

static void formatCoordinate(int32_t e7, char *buffer, size_t size) {
  double degrees = (double)e7 / 10000000.0;
  snprintf(buffer, size, "%.5f", degrees);
}

static int screenOutCode(int x, int y, int left, int top, int right, int bottom) {
  int code = 0;
  if (x < left) code |= 1;
  else if (x > right) code |= 2;
  if (y < top) code |= 4;
  else if (y > bottom) code |= 8;
  return code;
}

static bool clipScreenLine(int &x0, int &y0, int &x1, int &y1,
                           int left, int top, int right, int bottom) {
  int code0 = screenOutCode(x0, y0, left, top, right, bottom);
  int code1 = screenOutCode(x1, y1, left, top, right, bottom);
  while (true) {
    if (!(code0 | code1)) return true;
    if (code0 & code1) return false;
    int code = code0 ? code0 : code1;
    int x = 0;
    int y = 0;
    if (code & 8) {
      if (y1 == y0) return false;
      x = x0 + (x1 - x0) * (bottom - y0) / (y1 - y0);
      y = bottom;
    } else if (code & 4) {
      if (y1 == y0) return false;
      x = x0 + (x1 - x0) * (top - y0) / (y1 - y0);
      y = top;
    } else if (code & 2) {
      if (x1 == x0) return false;
      y = y0 + (y1 - y0) * (right - x0) / (x1 - x0);
      x = right;
    } else {
      if (x1 == x0) return false;
      y = y0 + (y1 - y0) * (left - x0) / (x1 - x0);
      x = left;
    }
    if (code == code0) {
      x0 = x;
      y0 = y;
      code0 = screenOutCode(x0, y0, left, top, right, bottom);
    } else {
      x1 = x;
      y1 = y;
      code1 = screenOutCode(x1, y1, left, top, right, bottom);
    }
  }
}

static void drawStyledMapLine(int x0, int y0, int x1, int y1, uint8_t style) {
#if ORM_WIREFRAME_LAYOUT
  int divisions = max(abs(x1 - x0), abs(y1 - y0));
  bool mostlyHorizontal = abs(x1 - x0) >= abs(y1 - y0);
  if (style == 0) {
    // Primary roads: a solid three-pixel ribbon with true perpendicular width.
    u8g2->drawLine(x0, y0, x1, y1);
    if (mostlyHorizontal) {
      u8g2->drawLine(x0, y0 - 1, x1, y1 - 1);
      u8g2->drawLine(x0, y0 + 1, x1, y1 + 1);
    } else {
      u8g2->drawLine(x0 - 1, y0, x1 - 1, y1);
      u8g2->drawLine(x0 + 1, y0, x1 + 1, y1);
    }
  } else if (style <= 2) {
    // Secondary roads and ordinary streets: continuous two-pixel vectors.
    u8g2->drawLine(x0, y0, x1, y1);
    if (mostlyHorizontal) u8g2->drawLine(x0, y0 + 1, x1, y1 + 1);
    else u8g2->drawLine(x0 + 1, y0, x1 + 1, y1);
  } else {
    // Non-road geometry keeps a recognizable pattern: cycleways use long
    // dashes, paths/service roads use sparse dots, and water uses a wide dash.
    int cadence = style == 3 ? 5 : (style == 6 ? 7 : 4);
    int run = style == 3 ? 3 : (style == 6 ? 4 : 1);
    for (int step = 0; step <= divisions; ++step) {
      if ((step % cadence) >= run) continue;
      int x = divisions ? x0 + (x1 - x0) * step / divisions : x0;
      int y = divisions ? y0 + (y1 - y0) * step / divisions : y0;
      u8g2->drawPixel(x, y);
    }
  }
#else
  if (style == 0) {
    u8g2->drawLine(x0, y0, x1, y1);
    u8g2->drawLine(x0 + 1, y0, x1 + 1, y1);
  } else if (style <= 2) {
    u8g2->drawLine(x0, y0, x1, y1);
  } else if (style == 3) {
    u8g2->drawLine(x0, y0, x1, y1);
    for (int step = 0; step <= 8; step += 2) {
      int x = x0 + (x1 - x0) * step / 8;
      int y = y0 + (y1 - y0) * step / 8;
      u8g2->drawDisc(x, y, 1);
    }
  } else {
    int divisions = max(abs(x1 - x0), abs(y1 - y0));
    for (int step = 0; step <= divisions; step += style == 6 ? 3 : 4) {
      int x = divisions ? x0 + (x1 - x0) * step / divisions : x0;
      int y = divisions ? y0 + (y1 - y0) * step / divisions : y0;
      u8g2->drawPixel(x, y);
    }
  }
#endif
}

static bool geoToMapScreen(double lon, double lat, double leftLon, double bottomLat,
                           double lonRange, double latRange, int mapX, int mapY,
                           int mapWidth, int mapHeight, int &x, int &y) {
  x = mapX + (int)((lon - leftLon) * mapWidth / lonRange);
  y = mapY + mapHeight - (int)((lat - bottomLat) * mapHeight / latRange);
  return x >= mapX && x < mapX + mapWidth && y >= mapY && y < mapY + mapHeight;
}

static void drawMiniMap(const LiveData &data, int mapX, int mapY, int mapWidth, int mapHeight) {
  u8g2->drawHLine(mapX, mapY, mapWidth);
  u8g2->drawHLine(mapX, mapY + mapHeight - 1, mapWidth);
  if (!data.hasGps) {
    u8g2->setFont(u8g2_font_6x12_tf);
    u8g2->drawStr(mapX + 8, mapY + mapHeight / 2, "WAITING FOR GPS");
    return;
  }

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

  if (firstCol <= lastCol && firstRow <= lastRow) {
    for (int row = firstRow; row <= lastRow; ++row) {
      for (int col = firstCol; col <= lastCol; ++col) {
        OrmMapTile tile;
        memcpy_P(&tile, &ORM_MAP_INDEX[row * ORM_MAP_COLS + col], sizeof(tile));
        uint32_t position = tile.offset;
        uint32_t end = tile.offset + tile.length;
        while (position + 2 <= end) {
          uint8_t style = pgm_read_byte(ORM_MAP_DATA + position++);
          uint8_t count = pgm_read_byte(ORM_MAP_DATA + position++);
          if (count < 2 || position + (uint32_t)count * 2 > end) break;
          int previousX = 0;
          int previousY = 0;
          for (uint8_t point = 0; point < count; ++point) {
            uint8_t localX = pgm_read_byte(ORM_MAP_DATA + position++);
            uint8_t localY = pgm_read_byte(ORM_MAP_DATA + position++);
            double lon = ORM_MAP_MIN_LON + col * ORM_MAP_TILE_LON +
                         localX * ORM_MAP_TILE_LON / 255.0;
            double lat = ORM_MAP_MIN_LAT + row * ORM_MAP_TILE_LAT +
                         localY * ORM_MAP_TILE_LAT / 255.0;
            int x = 0;
            int y = 0;
            geoToMapScreen(lon, lat, leftLon, bottomLat, lonRange, latRange,
                           mapX, mapY, mapWidth, mapHeight, x, y);
            if (point > 0) {
              int x0 = previousX;
              int y0 = previousY;
              int x1 = x;
              int y1 = y;
              if (clipScreenLine(x0, y0, x1, y1, mapX + 1, mapY + 1,
                                 mapX + mapWidth - 2, mapY + mapHeight - 2)) {
                drawStyledMapLine(x0, y0, x1, y1, style);
              }
            }
            previousX = x;
            previousY = y;
          }
        }
      }
    }
  }

  // Breadcrumb is deliberately thicker than all map geometry.
  if (data.trackCount >= 2) {
    uint16_t oldest = (data.trackHead + LiveData::TRACK_CAPACITY - data.trackCount) %
                      LiveData::TRACK_CAPACITY;
    int previousX = 0;
    int previousY = 0;
    bool previousVisible = false;
    for (uint16_t index = 0; index < data.trackCount; ++index) {
      uint16_t slot = (oldest + index) % LiveData::TRACK_CAPACITY;
      int x = 0;
      int y = 0;
      bool visible = geoToMapScreen(data.trackLongitudeE7[slot] / 10000000.0,
                                    data.trackLatitudeE7[slot] / 10000000.0,
                                    leftLon, bottomLat, lonRange, latRange,
                                    mapX, mapY, mapWidth, mapHeight, x, y);
      if (previousVisible && visible) {
        bool routeHorizontal = abs(x - previousX) >= abs(y - previousY);
        // Clear a five-pixel halo underneath the route so it stays distinct
        // even when it follows a thick primary road.
        u8g2->setDrawColor(0);
        if (routeHorizontal) {
          for (int offset = -2; offset <= 2; ++offset)
            u8g2->drawLine(previousX, previousY + offset, x, y + offset);
        } else {
          for (int offset = -2; offset <= 2; ++offset)
            u8g2->drawLine(previousX + offset, previousY, x + offset, y);
        }
        u8g2->setDrawColor(1);
        u8g2->drawLine(previousX, previousY, x, y);
        if (routeHorizontal) {
          u8g2->drawLine(previousX, previousY - 1, x, y - 1);
          u8g2->drawLine(previousX, previousY + 1, x, y + 1);
        } else {
          u8g2->drawLine(previousX - 1, previousY, x - 1, y);
          u8g2->drawLine(previousX + 1, previousY, x + 1, y);
        }
      }
      previousX = x;
      previousY = y;
      previousVisible = visible;
    }
  }

  int markerX = mapX + mapWidth / 2;
  int markerY = mapY + mapHeight / 2;
  double heading = data.headingCentidegrees == UNKNOWN_U16
                       ? 0.0
                       : data.headingCentidegrees / 100.0 * PI / 180.0;
  int tipX = markerX + (int)(sin(heading) * 8.0);
  int tipY = markerY - (int)(cos(heading) * 8.0);
#if ORM_WIREFRAME_LAYOUT
  // High-contrast current-position target: clear nearby map detail to form a
  // white halo, then add a solid black core, white centre and heading needle.
  u8g2->setDrawColor(0);
  u8g2->drawDisc(markerX, markerY, 11);
  u8g2->setDrawColor(1);
  u8g2->drawCircle(markerX, markerY, 10);
  u8g2->drawDisc(markerX, markerY, 7);
  u8g2->setDrawColor(0);
  u8g2->drawDisc(markerX, markerY, 2);
  u8g2->setDrawColor(1);
  u8g2->drawLine(markerX, markerY, tipX, tipY);
  u8g2->drawDisc(tipX, tipY, 1);
#else
  u8g2->setDrawColor(0);
  u8g2->drawDisc(markerX, markerY, 5);
  u8g2->setDrawColor(1);
  u8g2->drawCircle(markerX, markerY, 5);
  u8g2->drawLine(markerX, markerY, tipX, tipY);
#endif

  u8g2->setFont(u8g2_font_4x6_tf);
  u8g2->setDrawColor(0);
  int attributionWidth = u8g2->getStrWidth(ORM_MAP_ATTRIBUTION) + 4;
  u8g2->drawBox(mapX + 2, mapY + mapHeight - 8, attributionWidth, 7);
  u8g2->setDrawColor(1);
  u8g2->drawStr(mapX + 3, mapY + mapHeight - 2, ORM_MAP_ATTRIBUTION);
}

static uint32_t moodHash(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352du;
  x ^= x >> 15;
  x *= 0x846ca68bu;
  return x ^ (x >> 16);
}

static void drawMoodPanel(RideMood mood, int x, int y, int width, int height) {
  uint32_t frame = millis() / (mood == MOOD_PUSHING ? 110 : mood == MOOD_DESCENDING ? 170 : 260);
  uint8_t density = mood == MOOD_PUSHING ? 58 : mood == MOOD_CLIMBING ? 46 :
                    mood == MOOD_DESCENDING ? 36 : mood == MOOD_LOST ? 18 : 28;
  for (int gy = y + 3; gy < y + 10; gy += 4) {
    for (int gx = x + 3; gx < x + width - 2; gx += 5) {
      uint32_t noise = moodHash((gx * 131u) ^ (gy * 977u) ^ (frame * (mood + 3u)));
      if ((noise & 0xff) < density) u8g2->drawPixel(gx, gy);
    }
  }
  const char *label = moodName(mood);
  u8g2->setFont(u8g2_font_helvB10_tf);
  int labelX = x + (width - u8g2->getStrWidth(label)) / 2;
  u8g2->drawStr(labelX, y + height - 7, label);
  u8g2->drawHLine(x, y + height - 1, width);
}

static void drawHistoryScreen() {
  char value[96];
  u8g2->setFont(u8g2_font_helvB18_tf);
  u8g2->drawStr(10, 27, "RIDE MEMORY");
  u8g2->setFont(u8g2_font_helvB14_tf);
  if (currentClockHour == UNKNOWN_U8) snprintf(value, sizeof(value), "--:--");
  else snprintf(value, sizeof(value), "%02u:%02u", currentClockHour, currentClockMinute);
  drawTextRight(290, 25, value);
  u8g2->setFont(u8g2_font_6x12_tf);
  if (isnan(ambientTemperatureC) || isnan(ambientHumidityPercent))
    snprintf(value, sizeof(value), "SHTC3 --");
  else
    snprintf(value, sizeof(value), "%.1f C   %.0f%% RH", ambientTemperatureC, ambientHumidityPercent);
  u8g2->drawStr(10, 45, value);
  u8g2->drawHLine(8, 54, 284);

  if (rideHistory.count == 0) {
    u8g2->setFont(u8g2_font_helvB14_tf);
    drawTextCentered(150, 118, "NO SAVED RIDES");
    u8g2->setFont(u8g2_font_6x12_tf);
    drawTextCentered(150, 143, "Start a Garmin activity with Orm Live");
  } else {
    for (uint8_t index = 0; index < min((int)rideHistory.count, 4); ++index) {
      const RideSummary &ride = rideHistory.rides[index];
      int top = 61 + index * 68;
      u8g2->setFont(u8g2_font_helvB10_tf);
      snprintf(value, sizeof(value), "%u  %s", index + 1, sportName(ride.sport));
      u8g2->drawStr(10, top + 13, value);
      if (ride.startHour != UNKNOWN_U8) {
        snprintf(value, sizeof(value), "%02u:%02u", ride.startHour, ride.startMinute);
        drawTextRight(290, top + 13, value);
      }
      u8g2->setFont(u8g2_font_6x12_tf);
      snprintf(value, sizeof(value), "%.2f KM  %02lu:%02lu:%02lu  AVG %.1f KM/H",
               ride.distanceDecimeters / 10000.0,
               (unsigned long)(ride.durationSeconds / 3600),
               (unsigned long)((ride.durationSeconds / 60) % 60),
               (unsigned long)(ride.durationSeconds % 60),
               ride.averageSpeedCentimetersPerSecond == UNKNOWN_U16 ? 0.0 :
                   ride.averageSpeedCentimetersPerSecond * 0.036);
      u8g2->drawStr(10, top + 31, value);
      snprintf(value, sizeof(value), "HR %u/%u  UP %.0f M  %u KCAL",
               ride.averageHeartRate == UNKNOWN_U8 ? 0 : ride.averageHeartRate,
               ride.maxHeartRate == UNKNOWN_U8 ? 0 : ride.maxHeartRate,
               ride.ascentDecimeters == UNKNOWN_U16 ? 0.0 : ride.ascentDecimeters / 10.0,
               ride.calories == UNKNOWN_U16 ? 0 : ride.calories);
      u8g2->drawStr(10, top + 49, value);
      u8g2->drawHLine(10, top + 58, 280);
    }
  }
  drawMoodPanel(MOOD_STOPPED, 188, 346, 102, 36);
  u8g2->setFont(u8g2_font_6x12_tf);
  u8g2->drawStr(10, 397, "BLE ORM  |  OFFLINE MEMORY");
}

static void drawWaiting(const LiveData &data) {
  u8g2->setFont(u8g2_font_helvB24_tf);
  drawTextCentered(150, 62, "ORM LIVE");
  u8g2->drawHLine(10, 78, 280);

  u8g2->setFont(u8g2_font_helvB18_tf);
  if (!data.connected) {
    drawTextCentered(150, 151, "WAITING FOR GARMIN");
    u8g2->setFont(u8g2_font_helvR14_tf);
    drawTextCentered(150, 190, "Open an activity containing");
    drawTextCentered(150, 216, "the Orm Live data field");
    drawTextCentered(150, 260, "BLE: ORM");
  } else {
    drawTextCentered(150, 151, "GARMIN CONNECTED");
    u8g2->setFont(u8g2_font_helvR14_tf);
    drawTextCentered(150, 190, "Data starts with the activity");
    drawTextCentered(150, 216, "Keep ORM Live on a data screen");
  }

  u8g2->setFont(u8g2_font_6x12_tf);
  drawTextCentered(150, 388, "SERVICE D8185099  |  PROTOCOL V1");
}

#if 0
static void drawDashboardLegacy(const LiveData &data) {
  char value[48];
  RideMood mood = classifyMood(data);

  u8g2->setFont(u8g2_font_helvB14_tf);
  u8g2->drawStr(16, 27, sportName(data.sport));
  u8g2->setFont(u8g2_font_6x12_tf);
  u8g2->drawStr(16, 40, timerStateName(data.timerState));
  drawClockAndEnvironment(152, 20);
  drawMoodPanel(mood, 277, 5, 107, 36);
  u8g2->drawHLine(16, 43, 368);

  formatDuration(data.timerSeconds, value, sizeof(value));
  u8g2->setFont(u8g2_font_logisoso24_tn);
  u8g2->drawStr(16, 80, value);

  u8g2->setFont(u8g2_font_helvR10_tf);
  u8g2->drawStr(18, 106, "SPEED");
  u8g2->setFont(u8g2_font_logisoso38_tn);
  if (data.speedCentimetersPerSecond == UNKNOWN_U16) snprintf(value, sizeof(value), "--");
  else snprintf(value, sizeof(value), "%.1f", data.speedCentimetersPerSecond * 0.036);
  u8g2->drawStr(14, 157, value);
  u8g2->setFont(u8g2_font_6x12_tf);
  u8g2->drawStr(18, 174, "KM/H");

  drawMiniMap(data, 220, 54, 164, 126);

  u8g2->drawHLine(16, 191, 368);
  u8g2->setFont(u8g2_font_helvR10_tf);
  u8g2->drawStr(18, 211, "HEART");
  u8g2->drawStr(103, 211, "DISTANCE");
  u8g2->drawStr(210, 211, "CADENCE");
  u8g2->drawStr(309, 211, "POWER");
  u8g2->setFont(u8g2_font_helvB18_tf);
  if (data.heartRate == UNKNOWN_U8) snprintf(value, sizeof(value), "--");
  else snprintf(value, sizeof(value), "%u", data.heartRate);
  u8g2->drawStr(18, 239, value);
  drawOdometer(data.distanceDecimeters, 103, 218);
  if (data.cadence == UNKNOWN_U8) snprintf(value, sizeof(value), "CAD --");
  else snprintf(value, sizeof(value), "%u", data.cadence);
  u8g2->drawStr(210, 239, value);
  snprintf(value, sizeof(value), "--");
  u8g2->drawStr(309, 239, value);

  u8g2->setFont(u8g2_font_6x12_tf);
  if (data.heartRateZone == UNKNOWN_U8) snprintf(value, sizeof(value), "BPM  Z-");
  else snprintf(value, sizeof(value), "BPM  Z%u", data.heartRateZone);
  u8g2->drawStr(18, 253, value);
  u8g2->drawStr(103, 253, "KM");
  u8g2->drawStr(210, 253, "RPM");
  u8g2->drawStr(309, 253, "W");
  for (uint8_t zone = 1; zone <= 5; ++zone) {
    int x = 18 + (zone - 1) * 13;
    if (data.heartRateZone != UNKNOWN_U8 && zone <= data.heartRateZone) u8g2->drawBox(x, 258, 10, 5);
    else u8g2->drawFrame(x, 258, 10, 5);
  }

  uint32_t ageSeconds = data.lastPacketAt == 0 ? 0 : (millis() - data.lastPacketAt) / 1000;
  if (data.altitudeDecimeters == UNKNOWN_S16)
    snprintf(value, sizeof(value), "ALT --  GRADE %.1f%%  GPS Q:%u  AGE %lus",
             data.gradeTenthsPercent / 10.0, data.gpsQuality, (unsigned long)ageSeconds);
  else
    snprintf(value, sizeof(value), "ALT %.1f M  GPS %u  %lus",
             data.altitudeDecimeters / 10.0, data.gpsQuality,
             (unsigned long)ageSeconds);
  u8g2->drawStr(18, 279, value);
  if (!data.connected) drawTextRight(384, 279, "DISCONNECTED");
  else if (ageSeconds > 5) drawTextRight(384, 279, "DATA STALE");
  else drawTextRight(384, 279, "CONNECTED");
}
#endif

#if ORM_WIREFRAME_LAYOUT
#include "WireframeLayout.inc"
#if ORM_BUILD_DEMO
#include "DemoRide.inc"
#endif
#else
// Indoor cycling: the wheel speed a trainer reports is a simulation artefact
// and the map never moves, so the screen leads with power instead. The watch
// already tells us which it is, so there is nothing for the rider to configure.
static bool isIndoorCycling(const LiveData &data) {
  return data.subSport == SUB_SPORT_INDOOR_CYCLING ||
         data.subSport == SUB_SPORT_SPIN ||
         data.subSport == SUB_SPORT_VIRTUAL_ACTIVITY;
}

// Same segmented bar the heart-rate zone already uses, so both read the same
// way. Power uses Coggan's seven zones against heart rate's five.
static void drawZoneBar(int x, int y, uint8_t zone, uint8_t zoneCount, int segmentWidth) {
  for (uint8_t index = 1; index <= zoneCount; ++index) {
    int segmentX = x + (index - 1) * (segmentWidth + 5);
    u8g2->drawHLine(segmentX, y + 2, segmentWidth);
    if (zone != UNKNOWN_U8 && index <= zone) u8g2->drawHLine(segmentX, y, segmentWidth);
  }
}

static void drawDashboard(const LiveData &data) {
  char value[96];
  RideMood mood = classifyMood(data);

  // White canvas, black typography, and a strict portrait hierarchy.
  u8g2->setFont(u8g2_font_helvB14_tf);
  u8g2->drawStr(10, 20, sportName(data.sport));
  if (currentClockHour == UNKNOWN_U8) snprintf(value, sizeof(value), "--:--");
  else snprintf(value, sizeof(value), "%02u:%02u", currentClockHour, currentClockMinute);
  drawTextRight(290, 20, value);
  u8g2->setFont(u8g2_font_6x12_tf);
  u8g2->drawStr(10, 37, timerStateName(data.timerState));
  u8g2->drawHLine(8, 44, 284);

  drawMoodPanel(mood, 94, 48, 112, 27);
  const bool indoor = isIndoorCycling(data);
  u8g2->setFont(u8g2_font_helvB10_tf);
  drawSpeedIcon(12, 77);
  if (indoor) {
    u8g2->drawStr(32, 89, "POWER");
    // The zone sits beside the number so effort and target read together.
    if (data.powerZone != UNKNOWN_U8) {
      snprintf(value, sizeof(value), "Z%u", data.powerZone);
      drawTextRight(284, 89, value);
    } else {
      drawTextRight(284, 89, "Z-");
    }
    u8g2->setFont(u8g2_font_logisoso38_tn);
    if (data.powerWatts == UNKNOWN_U16) snprintf(value, sizeof(value), "--");
    else snprintf(value, sizeof(value), "%u", data.powerWatts);
    drawTextCentered(150, 138, value);
    u8g2->setFont(u8g2_font_helvB10_tf);
    u8g2->drawStr(238, 136, "W");
    drawZoneBar(150, 145, data.powerZone, 7, 14);
  } else {
    u8g2->drawStr(32, 89, "CURRENT SPEED");
    u8g2->setFont(u8g2_font_logisoso38_tn);
    if (data.speedCentimetersPerSecond == UNKNOWN_U16) snprintf(value, sizeof(value), "--");
    else snprintf(value, sizeof(value), "%.1f", data.speedCentimetersPerSecond * 0.036);
    drawTextCentered(150, 138, value);
    u8g2->setFont(u8g2_font_helvB10_tf);
    u8g2->drawStr(238, 136, "KM/H");
  }

  u8g2->drawHLine(8, 149, 284);
  u8g2->drawVLine(150, 157, 57);
  u8g2->drawHLine(8, 221, 284);
  u8g2->setFont(u8g2_font_helvB10_tf);
  drawPinIcon(16, 155);
  u8g2->drawStr(36, 168, "DISTANCE");
  drawHeartIcon(162, 154);
  u8g2->drawStr(184, 168, "HEART RATE");
  u8g2->setFont(u8g2_font_helvB18_tf);
  snprintf(value, sizeof(value), "%.2f", data.distanceDecimeters / 10000.0);
  u8g2->drawStr(16, 204, value);
  u8g2->setFont(u8g2_font_6x12_tf);
  drawTextRight(138, 204, "KM");
  u8g2->setFont(u8g2_font_helvB18_tf);
  if (data.heartRate == UNKNOWN_U8) snprintf(value, sizeof(value), "--");
  else snprintf(value, sizeof(value), "%u", data.heartRate);
  u8g2->drawStr(162, 204, value);
  u8g2->setFont(u8g2_font_6x12_tf);
  if (data.heartRateZone == UNKNOWN_U8) snprintf(value, sizeof(value), "BPM  Z-");
  else snprintf(value, sizeof(value), "BPM  Z%u", data.heartRateZone);
  drawTextRight(284, 204, value);
  drawZoneBar(162, 212, data.heartRateZone, 5, 18);

  drawClockIcon(10, 227);
  u8g2->setFont(u8g2_font_helvB10_tf);
  u8g2->drawStr(29, 241, "DURATION");
  u8g2->drawStr(119, 241, "CADENCE");
  // Indoors the big number is already power, so the third cell shows speed
  // instead of repeating it.
  u8g2->drawStr(215, 241, indoor ? "SPEED" : "POWER");
  formatDuration(data.timerSeconds, value, sizeof(value));
  u8g2->setFont(u8g2_font_helvB14_tf);
  u8g2->drawStr(10, 265, value);
  if (data.cadenceRpm == UNKNOWN_U8) snprintf(value, sizeof(value), "--");
  else snprintf(value, sizeof(value), "%u", data.cadenceRpm);
  u8g2->drawStr(119, 265, value);
  u8g2->setFont(u8g2_font_6x12_tf);
  u8g2->drawStr(166, 265, "RPM");
  u8g2->setFont(u8g2_font_helvB14_tf);
  if (indoor) {
    if (data.speedCentimetersPerSecond == UNKNOWN_U16) snprintf(value, sizeof(value), "--");
    else snprintf(value, sizeof(value), "%.1f", data.speedCentimetersPerSecond * 0.036);
  } else {
    if (data.powerWatts == UNKNOWN_U16) snprintf(value, sizeof(value), "--");
    else snprintf(value, sizeof(value), "%u", data.powerWatts);
  }
  u8g2->drawStr(215, 265, value);
  u8g2->setFont(u8g2_font_6x12_tf);
  drawTextRight(290, 265, indoor ? "KM/H" : "W");

  drawMiniMap(data, 8, 276, 284, 92);

  uint32_t ageSeconds = data.lastPacketAt == 0 ? 0 : (millis() - data.lastPacketAt) / 1000;
  // Ascent and grade are gone from this row: indoors they are always zero, and
  // outdoors they lost their place to cadence and power.
  if (indoor)
    snprintf(value, sizeof(value), "TRAINER  %lus", (unsigned long)ageSeconds);
  else if (data.altitudeDecimeters == UNKNOWN_S16)
    snprintf(value, sizeof(value), "ALT --  GPS %u  %lus",
             data.gpsQuality, (unsigned long)ageSeconds);
  else
    snprintf(value, sizeof(value), "ALT %.0fM  GPS %u  %lus",
             data.altitudeDecimeters / 10.0, data.gpsQuality,
             (unsigned long)ageSeconds);
  u8g2->setFont(u8g2_font_4x6_tf);
  u8g2->drawStr(8, 382, value);
  if (!data.connected) drawTextRight(292, 394, "DISCONNECTED");
  else if (ageSeconds > 5) drawTextRight(292, 394, "DATA STALE");
  else drawTextRight(292, 394, "CONNECTED");
}
#endif

static void redraw() {
  LiveData snapshot;
  portENTER_CRITICAL(&dataMux);
  snapshot = liveData;
  portEXIT_CRITICAL(&dataMux);

  u8g2->clearBuffer();
  u8g2->setDrawColor(1);
  if (!snapshot.connected) drawHistoryScreen();
  else if (!snapshot.hasActivity) drawWaiting(snapshot);
  else drawDashboard(snapshot);

  // On the reflective ST7305 panel a cleared U8g2 buffer is physically black.
  // Invert the completed monochrome frame so the UI is black ink on white.
  u8g2->setDrawColor(2);
  u8g2->drawBox(0, 0, LCD_WIDTH, LCD_HEIGHT);
  u8g2->setDrawColor(1);
  u8g2->sendBuffer();
  lastDrawAt = millis();
  screenDirty = false;
}

static void startBleServer() {
  BLEDevice::init(DEVICE_NAME);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());
  bleServer->advertiseOnDisconnect(true);

  BLEService *service = bleServer->createService(SERVICE_UUID);
  BLECharacteristic *telemetry = service->createCharacteristic(
      TELEMETRY_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  telemetry->setCallbacks(new TelemetryCallbacks());
  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE advertising as ORM (open transport)");
}

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println("Orm Garmin Live Display boot");

  loadRideHistory();
  bool boardReady = board.begin(boardI2c);
  Serial.printf("Board sensors/rtc: %s\n", boardReady ? "ready" : "unavailable");
  board.readClock(currentClockHour, currentClockMinute, currentClockSecond);
  if (board.readEnvironment(ambientTemperatureC, ambientHumidityPercent))
    lastEnvironmentReadAt = millis();

  lcd.begin(0, U8G2_R0);
  u8g2 = lcd.getU8g2();
  u8g2->setFontMode(1);
  redraw();
#if ORM_BUILD_DEMO
  demoRideBegin();
#else
  startBleServer();
#endif
}

void loop() {
#if ORM_BUILD_DEMO
  demoRideUpdate();
#endif
  uint32_t now = millis();

  // Decision de energia. Se calcula y se reporta, pero NO se duerme todavia:
  // la unica fuente de despertar prevista es la interrupcion de movimiento del
  // MPU6050, que aun no esta instalado. Entrar en sueño profundo sin ella
  // dejaria la placa apagada hasta un reset manual -- peor que no ahorrar nada.
  // Cuando exista el driver, aca va el esp_deep_sleep_start().
  {
    static orm::PowerState reportedPowerState = orm::PowerState::Active;
    portENTER_CRITICAL(&dataMux);
    LiveData snapshot = liveData;
    portEXIT_CRITICAL(&dataMux);
    orm::PowerState state = currentPowerState(snapshot, now);
    if (state != reportedPowerState) {
      reportedPowerState = state;
      Serial.printf("Power state: %s\n",
                    state == orm::PowerState::Asleep
                        ? "idle (would sleep once the accelerometer can wake us)"
                        : "active");
    }
  }

  if (clockSyncPending) {
    portENTER_CRITICAL(&dataMux);
    uint8_t hour = pendingClockHour;
    uint8_t minute = pendingClockMinute;
    uint8_t second = pendingClockSecond;
    clockSyncPending = false;
    portEXIT_CRITICAL(&dataMux);
    if (board.setClock(hour, minute, second)) {
      currentClockHour = hour;
      currentClockMinute = minute;
      currentClockSecond = second;
      screenDirty = true;
    }
  }
  if (now - lastClockReadAt >= 1000) {
    lastClockReadAt = now;
    uint8_t hour, minute, second;
    if (board.readClock(hour, minute, second)) {
      currentClockHour = hour;
      currentClockMinute = minute;
      currentClockSecond = second;
    }
  }
  if (lastEnvironmentReadAt == 0 || now - lastEnvironmentReadAt >= 30000) {
    lastEnvironmentReadAt = now;
    float temperature, humidity;
    if (board.readEnvironment(temperature, humidity)) {
      ambientTemperatureC = temperature;
      ambientHumidityPercent = humidity;
      screenDirty = true;
    }
  }
  if (historySavePending) saveRideHistory();

  // Redraw on new data, periodically so packet age remains truthful, and at a
  // higher rate only during the short >30 km/h acceleration takeover.
  uint32_t periodicRedrawMs = 1000;
#if ORM_WIREFRAME_LAYOUT
  periodicRedrawMs = wireAnimationIntervalMs();
#endif
  if ((screenDirty && now - lastDrawAt >= 150) || now - lastDrawAt >= periodicRedrawMs) {
    redraw();
  }
  delay(10);
}

#pragma once
#include <Arduino.h>
// Synthetic Jarun-area sample used for deterministic demos.
#define ORM_MAP_ATTRIBUTION "SAMPLE"
struct OrmMapTile { uint32_t offset; uint32_t length; };
static constexpr double ORM_MAP_MIN_LON = 15.90000000;
static constexpr double ORM_MAP_MIN_LAT = 45.75000000;
static constexpr double ORM_MAP_TILE_LON = 0.10000000;
static constexpr double ORM_MAP_TILE_LAT = 0.07000000;
static constexpr uint16_t ORM_MAP_COLS = 1;
static constexpr uint16_t ORM_MAP_ROWS = 1;
static const OrmMapTile ORM_MAP_INDEX[1] PROGMEM = {{0u,98u}};
static const uint8_t ORM_MAP_DATA[98] PROGMEM = {0,5,0,168,64,168,127,171,191,182,255,189,1,5,36,0,46,73,54,146,59,219,64,255,1,5,0,120,64,124,127,127,191,127,255,131,2,4,31,95,64,109,102,117,143,120,2,4,20,84,66,87,115,91,166,102,3,7,41,95,71,73,107,73,140,95,107,117,66,117,41,95,4,4,51,66,87,58,125,73,135,102,6,7,46,102,76,84,112,84,138,98,115,113,76,113,46,102};
static constexpr uint32_t ORM_MAP_FEATURES = 8u;
static constexpr uint32_t ORM_MAP_BYTES = 98u;

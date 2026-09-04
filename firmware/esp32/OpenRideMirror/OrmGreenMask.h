#pragma once
#include <Arduino.h>
// Synthetic Jarun-area sample land-use mask used for deterministic demos.
static constexpr double ORM_GREEN_MIN_LON = 15.90000000;
static constexpr double ORM_GREEN_MIN_LAT = 45.75000000;
static constexpr double ORM_GREEN_CELL_LON = 0.00500000;
static constexpr double ORM_GREEN_CELL_LAT = 0.00500000;
static constexpr uint16_t ORM_GREEN_COLS = 20;
static constexpr uint16_t ORM_GREEN_ROWS = 14;
static const uint8_t ORM_GREEN_MASK[35] PROGMEM = {240,135,207,255,249,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,127,192,255,1,240,7,0,0,0,0,0,0,0,0};

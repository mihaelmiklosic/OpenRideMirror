#pragma once
#include <Arduino.h>
// Synthetic Jarun-area sample labels used for deterministic demos.
struct __attribute__((packed)) OrmMapLabelTile { uint16_t start; uint8_t count; };
struct __attribute__((packed)) OrmMapLabel { int32_t lonE7; int32_t latE7; uint32_t text; uint8_t style; };
static const OrmMapLabelTile ORM_LABEL_INDEX[1] PROGMEM = {{0,4}};
static const OrmMapLabel ORM_LABELS[4] PROGMEM = {{159300000,457830000,0,2},{159310000,457770000,18,16},{159170000,457890000,34,1},{159430000,457790000,53,3}};
static const char ORM_LABEL_TEXT[70] PROGMEM = {72,111,114,118,97,99,97,110,115,107,97,32,99,101,115,116,97,0,74,97,114,117,110,115,107,111,32,106,101,122,101,114,111,0,90,97,103,114,101,98,97,99,107,97,32,97,118,101,110,105,106,97,0,65,108,101,106,97,32,77,46,32,76,106,117,98,101,107,97,0};
static constexpr uint16_t ORM_LABEL_COUNT = 4;
static constexpr uint32_t ORM_LABEL_BYTES = 70;

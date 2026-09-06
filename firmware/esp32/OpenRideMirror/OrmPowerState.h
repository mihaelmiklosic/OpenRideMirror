// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// When the receiver may sleep, and when it must stay awake.
//
// Deliberately free of Arduino and ESP-IDF headers so the decision can be
// compiled and exercised on a workstation. The rules are cheap to state and
// expensive to get wrong: sleeping while the rider is looking at the screen is
// a bug they will notice immediately, and waking costs a full BLE
// reconnection -- measured at 6.4 s against a real Fenix 8, before adding board
// boot and the first RLCD refresh.
//
// That cost is why "not moving" alone is not enough to sleep. Stopping at a
// light, adjusting a shoe or waiting at a crossing all leave the bike still
// while the ride is very much in progress.

#include <stdint.h>

namespace orm {

enum class PowerState {
  Active,  // Screen refreshing, BLE connected.
  Asleep,  // Deep sleep, woken by the accelerometer's motion interrupt.
};

// Five minutes. Long enough that a coffee stop or a roadside repair does not
// cost a reconnection, short enough that a bike parked in a garage stops
// draining the battery within minutes rather than hours.
static const uint32_t ORM_SLEEP_AFTER_STILL_MS = 5UL * 60UL * 1000UL;

struct PowerInputs {
  uint32_t nowMs = 0;
  // Last time the accelerometer reported movement. Zero means "never seen
  // movement since boot", which is treated as movement at boot so a receiver
  // powered on at a standstill does not immediately sleep.
  uint32_t lastMotionMs = 0;
  // The watch is connected AND its activity timer is running. While that holds
  // the rider is mid-ride and the screen must stay live no matter how still
  // the bike is.
  bool rideInProgress = false;
};

inline uint32_t millisSince(uint32_t nowMs, uint32_t thenMs) {
  // millis() wraps after ~49 days. Unsigned subtraction wraps with it, so this
  // stays correct across the rollover instead of producing a huge interval that
  // would put the board to sleep mid-ride.
  return nowMs - thenMs;
}

inline PowerState decidePowerState(const PowerInputs &inputs) {
  if (inputs.rideInProgress) return PowerState::Active;
  if (inputs.lastMotionMs == 0) return PowerState::Active;
  if (millisSince(inputs.nowMs, inputs.lastMotionMs) < ORM_SLEEP_AFTER_STILL_MS)
    return PowerState::Active;
  return PowerState::Asleep;
}

}  // namespace orm

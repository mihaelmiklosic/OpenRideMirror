// SPDX-License-Identifier: GPL-3.0-only
//
// Ejercita OrmPowerState.h en la máquina de desarrollo. El header no incluye
// nada de Arduino ni de ESP-IDF justamente para que esto sea posible: la
// decisión de dormir no se puede probar en la placa hasta que llegue, y es una
// decisión que si se equivoca apaga la pantalla en medio de una salida.
#include <cstdio>
#include <cstdlib>

#include "OrmPowerState.h"

using orm::PowerInputs;
using orm::PowerState;
using orm::decidePowerState;
using orm::ORM_SLEEP_AFTER_STILL_MS;

static int failures = 0;

static void check(bool condition, const char *what) {
  if (!condition) {
    std::printf("FALLO: %s\n", what);
    failures += 1;
  }
}

static PowerState decide(uint32_t nowMs, uint32_t lastMotionMs, bool riding) {
  PowerInputs inputs;
  inputs.nowMs = nowMs;
  inputs.lastMotionMs = lastMotionMs;
  inputs.rideInProgress = riding;
  return decidePowerState(inputs);
}

int main() {
  const uint32_t FIVE_MIN = ORM_SLEEP_AFTER_STILL_MS;

  // Moverse mantiene la pantalla viva.
  check(decide(1000, 900, false) == PowerState::Active,
        "movimiento reciente debe mantener despierto");

  // El umbral: justo antes sigue despierto, justo en el limite duerme.
  check(decide(FIVE_MIN + 999, 1000, false) == PowerState::Active,
        "un milisegundo antes de los 5 min todavia esta despierto");
  check(decide(FIVE_MIN + 1000, 1000, false) == PowerState::Asleep,
        "a los 5 min exactos debe dormir");

  // La regla que mas importa: parado en un semaforo con la actividad corriendo
  // NO se puede apagar, por mucho que la bici lleve quieta.
  check(decide(FIVE_MIN * 10, 1000, true) == PowerState::Active,
        "nunca dormir con la actividad corriendo");
  check(decide(0xFFFFFFFFu, 1, true) == PowerState::Active,
        "la actividad corriendo gana sobre cualquier tiempo quieto");

  // Arranque: una placa encendida estando quieta no debe dormirse de inmediato
  // antes de haber visto un solo movimiento.
  check(decide(FIVE_MIN * 3, 0, false) == PowerState::Active,
        "sin lecturas del acelerometro todavia, quedarse despierto");

  // millis() da la vuelta a los ~49 dias. Si la resta se hiciera mal, el
  // intervalo saldria gigante y la placa se dormiria en plena salida.
  // Ojo con la aritmetica: desde 0xFFFFFFFF-999 quedan 1000 valores hasta
  // volver a 0 (el propio 0xFFFFFFFF cuenta), no 999.
  const uint32_t NEAR_WRAP = 0xFFFFFFFFu - 999;    // 1000 ms antes del vuelco
  const uint32_t AFTER_WRAP = 2000;                // 2000 ms despues del vuelco
  check(decide(AFTER_WRAP, NEAR_WRAP, false) == PowerState::Active,
        "el vuelco de millis no debe provocar un apagado");
  check(orm::millisSince(AFTER_WRAP, NEAR_WRAP) == 3000,
        "millisSince debe dar 3000 ms cruzando el vuelco");

  // Y cruzando el vuelco tambien tiene que poder dormir cuando corresponde.
  check(decide(FIVE_MIN, NEAR_WRAP, false) == PowerState::Asleep,
        "cruzando el vuelco, 5 min quieto sigue durmiendo");

  if (failures == 0) {
    std::printf("OK: todas las comprobaciones de estado de energia pasaron\n");
    return 0;
  }
  std::printf("%d comprobacion(es) fallaron\n", failures);
  return 1;
}

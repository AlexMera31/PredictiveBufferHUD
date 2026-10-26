// Estado compartido entre el motor del buffer (PredictiveBuffer.cpp) y el HUD (main.cpp).
// Antes el HUD lo leia/escribia por direcciones fijas dentro de ai.predictivebuffer.dll;
// ahora es una simple variable global dentro del mismo mod.
#pragma once
#include <cstdint>

namespace pbuf {
    constexpr int OFF_IDX = 9;      // idx == OFF_IDX -> buffer desactivado; cualquier otro valor -> activado

    struct Shared {
        uint32_t idx   = 0;         // 0 = activado, OFF_IDX = desactivado
        uint8_t  flag  = 1;         // 1 = el motor aplica el buffer ahora mismo (lo escribe el HUD cada frame)
        uint32_t all   = 2;         // limite "All" en ticks (todo menos wave)
        uint32_t wave  = 2;         // limite "Wave" en ticks
        int32_t  last  = -1;        // ticks ahorrados en el ultimo click bufferizado (-1 = ninguno)
        uint8_t  press = 1;         // 1 = PRESS, 0 = RELEASE (del ultimo click bufferizado)
        uint32_t seq   = 0;         // contador: sube en cada click bufferizado (el HUD detecta eventos con esto)
        uint8_t  orbs   = 1;        // 1 = el simulador tiene en cuenta orbes y pads
        uint8_t  moving = 1;        // 1 = el simulador predice objetos movidos por Move Triggers
    };

    inline Shared g;
}

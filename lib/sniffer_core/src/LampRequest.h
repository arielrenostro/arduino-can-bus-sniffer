#pragma once

#include <stdint.h>

namespace sniffer {

// Quadro 0x488 (ENGINE_DATA, ECU -> BSI, 100 ms). Byte 4: bit 1 =
// PREHEAT_LAMP_REQUEST (lâmpada de velas), bit 2 = MIL_REQUEST (luz de injeção).
static const uint16_t kLampFrameId = 0x488;
static const uint8_t kLampMinLen = 4;
static const uint8_t kLampPreheatMask = 0x02;
static const uint8_t kLampMilMask = 0x04;

// Estado da substituição de lâmpadas, compartilhado entre o parser de comandos
// (que altera) e o laço principal (que reage ao 0x488 da ECU). Cada campo é
// -1 (seguir o valor da ECU), 0 ou 1.
struct LampState {
  bool active;
  int8_t preheat;
  int8_t mil;
  uint32_t sent;    // cópias enfileiradas no controlador
  uint32_t failed;  // tentativas sem buffer de envio livre

  LampState() : active(false), preheat(-1), mil(-1), sent(0), failed(0) {}

  // `l off`: desliga e devolve os campos à ECU.
  void clear() {
    active = false;
    preheat = -1;
    mil = -1;
  }
};

inline void applyLampBit(uint8_t& byte, uint8_t mask, int8_t value) {
  if (value == 0) byte = (uint8_t)(byte & ~mask);
  else if (value == 1) byte = (uint8_t)(byte | mask);
}

// Copia o quadro recebido (`len` bytes, 4 a 8) para `out`, ajustando só os bits
// comandados do byte 4. Todo o resto fica como a ECU enviou.
inline bool buildLampFrame(const uint8_t* src, uint8_t len, int8_t preheat, int8_t mil,
                           uint8_t* out) {
  if (len < kLampMinLen || len > 8) return false;
  for (uint8_t i = 0; i < len; i++) out[i] = src[i];
  applyLampBit(out[3], kLampPreheatMask, preheat);
  applyLampBit(out[3], kLampMilMask, mil);
  return true;
}

}  // namespace sniffer

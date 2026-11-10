#pragma once

#include <stdint.h>

namespace sniffer {

// Quadro 0x388 (VEHICLE_DYNAMICS_ENGINE): bytes 1-2 velocidade, 3-4 contador de
// distância, 5 aceleração longitudinal. Neste carro (sem ABS nem caixa
// automática) é a única fonte de velocidade da BSI e vem da ECU.
static const uint16_t kSpeedFrameId = 0x388;
static const uint16_t kMaxKmh = 300;

// Estado da substituição de velocidade, compartilhado entre o parser de
// comandos (que altera) e o laço principal (que reage ao 0x388 da ECU).
struct SpeedState {
  bool active;
  uint16_t kmh;
  uint32_t sent;    // cópias enfileiradas no controlador
  uint32_t failed;  // tentativas sem buffer de envio livre

  SpeedState() : active(false), kmh(0), sent(0), failed(0) {}
};

// Copia o quadro recebido (`len` bytes, 2 a 8) para `out`, trocando só os bytes
// 1-2 pela velocidade (km/h x 100, 16 bits big-endian). O contador de distância
// e a aceleração ficam exatamente como a ECU os enviou. Escala (km/h x 0,01)
// das definições do PSA-RE para AEE2004/AEE2010; não confirmada para o AEE2001.
inline bool buildSpeedFrame(const uint8_t* src, uint8_t len, uint16_t kmh, uint8_t* out) {
  if (len < 2 || len > 8 || kmh > 655) return false;
  for (uint8_t i = 0; i < len; i++) out[i] = src[i];
  const uint16_t raw = (uint16_t)(kmh * 100u);
  out[0] = (uint8_t)(raw >> 8);
  out[1] = (uint8_t)(raw & 0xFF);
  return true;
}

}  // namespace sniffer

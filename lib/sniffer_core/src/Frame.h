#pragma once

#include <stdint.h>

namespace sniffer {

// Quadro CAN de dados com identificador padrão (11 bits), 0 a 8 bytes.
// ms = millis() no instante em que o quadro foi lido do controlador.
struct Frame {
  uint32_t ms;
  uint16_t id;
  uint8_t len;
  uint8_t data[8];
};

// Contadores exibidos pelo comando `s`.
struct Stats {
  uint32_t received;  // todos os quadros recebidos (exibidos ou não)
  uint32_t shown;     // quadros que passaram pelo filtro
  uint32_t lost;      // quadros perdidos (ring cheio ou estouro do controlador)
};

}  // namespace sniffer

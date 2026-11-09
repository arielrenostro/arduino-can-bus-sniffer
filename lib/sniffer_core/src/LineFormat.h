#pragma once

#include <stdint.h>

#include "Frame.h"
#include "Text.h"

namespace sniffer {

// Pior caso: 10 (ms) + 1 + 3 (ID) + 1 + 3 ("[8]") + 8*3 (" XX") + 1 ('\n') = 43,
// mais o NUL final.
static const uint8_t kMaxFrameLine = 44;

// Formata `<ms> <ID> [<n>] <b1> <b2> ...\n` em `out` (>= kMaxFrameLine bytes).
// Devolve o comprimento sem o NUL (o NUL é escrito para facilitar testes).
inline uint8_t formatFrame(const Frame& f, char* out) {
  char* p = out;
  p = text::putUint(p, f.ms);
  *p++ = ' ';
  p = text::putHex(p, f.id & 0x7FF, 3);
  *p++ = ' ';
  *p++ = '[';
  uint8_t n = f.len > 8 ? 8 : f.len;
  *p++ = (char)('0' + n);
  *p++ = ']';
  for (uint8_t i = 0; i < n; i++) {
    *p++ = ' ';
    p = text::putHex(p, f.data[i], 2);
  }
  *p++ = '\n';
  *p = '\0';
  return (uint8_t)(p - out);
}

}  // namespace sniffer

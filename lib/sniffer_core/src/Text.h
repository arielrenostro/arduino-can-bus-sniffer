#pragma once

#include <stdint.h>

// Montagem de texto sem printf/String (economiza flash e RAM no AVR).
// Cada função escreve em `p` e devolve o ponteiro logo após o que escreveu.
namespace sniffer {
namespace text {

inline char* putStr(char* p, const char* s) {
  while (*s) *p++ = *s++;
  return p;
}

// Decimal sem zeros à esquerda.
inline char* putUint(char* p, uint32_t v) {
  char tmp[10];
  uint8_t n = 0;
  do {
    tmp[n++] = (char)('0' + (v % 10));
    v /= 10;
  } while (v);
  while (n) *p++ = tmp[--n];
  return p;
}

// Hexadecimal maiúsculo com exatamente `digits` dígitos (completa com zeros).
inline char* putHex(char* p, uint16_t v, uint8_t digits) {
  static const char kHex[] = "0123456789ABCDEF";
  for (uint8_t i = digits; i > 0; i--) *p++ = kHex[(v >> ((i - 1) * 4)) & 0xF];
  return p;
}

}  // namespace text
}  // namespace sniffer

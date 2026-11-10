#pragma once

#include <stdint.h>

#include "Flash.h"

// Montagem de texto sem printf/String (economiza flash e RAM no AVR).
// Cada função escreve em `p` e devolve o ponteiro logo após o que escreveu.
namespace sniffer {
namespace text {

inline char* putStr(char* p, const char* s) {
  while (*s) *p++ = *s++;
  return p;
}

// Igual a putStr, para texto na flash (use com PSTR("...")).
inline char* putStrP(char* p, const char* flash) {
  char c;
  while ((c = (char)pgm_read_byte(flash++)) != '\0') *p++ = c;
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

// Decimal com sinal.
inline char* putInt(char* p, long v) {
  if (v < 0) {
    *p++ = '-';
    return putUint(p, (uint32_t)(-v));
  }
  return putUint(p, (uint32_t)v);
}

// Hexadecimal maiúsculo com exatamente `digits` dígitos (completa com zeros).
inline char* putHex(char* p, uint16_t v, uint8_t digits) {
  static const char kHex[] = "0123456789ABCDEF";
  for (uint8_t i = digits; i > 0; i--) *p++ = kHex[(v >> ((i - 1) * 4)) & 0xF];
  return p;
}

}  // namespace text
}  // namespace sniffer

#pragma once

#include <stdint.h>

// Textos fixos ficam na flash no AVR (a RAM do Nano é de só 2 KB). No PC (testes
// nativos) PSTR é a própria string.
#ifdef __AVR__
#include <avr/pgmspace.h>
#else
#ifndef PSTR
#define PSTR(s) (s)
#endif
#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*(const uint8_t*)(addr))
#endif
#endif

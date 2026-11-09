#pragma once

#include <stdint.h>

#include "Frame.h"
#include "IdFilter.h"
#include "Text.h"

namespace sniffer {

// Recebe uma linha de texto de resposta (sem '\n' no fim, sempre começando
// com '#'). Quem chama decide para onde vai (Serial, buffer de teste...).
typedef void (*EmitFn)(const char* text);

// Interpreta comandos digitados no monitor serial, um caractere por vez, sem
// bloquear:
//   f <id> [<id> ...]   substitui a lista de IDs exibidos
//   f +<id> [+<id> ...] adiciona IDs     f -<id> [-<id> ...]  remove IDs
//   f off               exibe todos os IDs
//   s                   estado do filtro e contadores
// IDs em hexadecimal, com ou sem prefixo 0x, de 0 a 7FF. Um comando de filtro
// que falha não altera a lista. Toda resposta começa com '#'.
template <uint8_t FN>
class CommandProcessor {
 public:
  static const uint8_t kLineMax = 48;

  CommandProcessor(IdFilter<FN>& filter, const Stats& stats, EmitFn emit)
      : filter_(filter), stats_(stats), emit_(emit), len_(0), tooLong_(false) {}

  void feed(char c) {
    if (c == '\r' || c == '\n') {
      finishLine();
      return;
    }
    if (tooLong_) return;
    if (len_ >= kLineMax) {
      tooLong_ = true;
      return;
    }
    line_[len_++] = c;
  }

 private:
  void finishLine() {
    if (tooLong_) {
      tooLong_ = false;
      len_ = 0;
      emit_("# erro: linha muito longa");
      return;
    }
    if (len_ == 0) return;
    line_[len_] = '\0';
    len_ = 0;
    execute(line_);
  }

  void execute(char* line) {
    char* tok[kLineMax / 2 + 1];
    uint8_t n = 0;
    char* p = line;
    while (*p) {
      while (*p == ' ' || *p == '\t') *p++ = '\0';
      if (!*p) break;
      tok[n++] = p;
      while (*p && *p != ' ' && *p != '\t') ++p;
    }
    if (n == 0) return;

    if (tok[0][1] == '\0') {
      char c = (char)(tok[0][0] | 0x20);  // minúscula
      if (c == 'f') return filterCommand(tok + 1, (uint8_t)(n - 1));
      if (c == 's') return statsCommand();
    }
    emit_("# erro: comando desconhecido (use f ou s)");
  }

  void filterCommand(char** a, uint8_t n) {
    if (n == 0) {
      emit_("# erro: uso: f <id>... | f +<id> | f -<id> | f off");
      return;
    }
    if (n == 1 && equalsNoCase(a[0], "off")) {
      filter_.clear();
      reportFilter();
      return;
    }

    const bool signedMode = isSign(a[0][0]);
    if (!signedMode && n > FN) {
      tooMany();
      return;
    }

    // Trabalha numa cópia: só confirma se todos os argumentos forem válidos.
    IdFilter<FN> next = filter_;
    if (!signedMode) next.clear();

    for (uint8_t i = 0; i < n; i++) {
      const char* t = a[i];
      const bool hasSign = isSign(t[0]);
      if (hasSign != signedMode) {
        emit_("# erro: nao misture IDs com e sem sinal (+/-)");
        return;
      }
      char sign = 0;
      if (hasSign) sign = *t++;
      uint16_t id;
      if (!parseId(t, id)) {
        emit_("# erro: ID invalido (hexadecimal, no maximo 7FF)");
        return;
      }
      FilterResult r = (sign == '-') ? next.remove(id) : next.add(id);
      if (r == FilterResult::TooMany) {
        tooMany();
        return;
      }
    }
    filter_ = next;
    reportFilter();
  }

  void statsCommand() {
    reportFilter();
    reportCounter("# recebidos = ", stats_.received);
    reportCounter("# exibidos = ", stats_.shown);
    reportCounter("# perdidos = ", stats_.lost);
  }

  void reportFilter() {
    if (!filter_.active()) {
      emit_("# filtro: desativado");
      return;
    }
    char buf[10 + 4 * FN];
    char* p = text::putStr(buf, "# filtro:");
    for (uint8_t i = 0; i < filter_.count(); i++) {
      *p++ = ' ';
      p = text::putHex(p, filter_.at(i), 3);
    }
    *p = '\0';
    emit_(buf);
  }

  void reportCounter(const char* label, uint32_t value) {
    char buf[32];
    char* p = text::putStr(buf, label);
    p = text::putUint(p, value);
    *p = '\0';
    emit_(buf);
  }

  void tooMany() {
    char buf[48];
    char* p = text::putStr(buf, "# erro: capacidade excedida (maximo ");
    p = text::putUint(p, FN);
    p = text::putStr(p, " IDs)");
    *p = '\0';
    emit_(buf);
  }

  static bool isSign(char c) { return c == '+' || c == '-'; }

  static bool equalsNoCase(const char* s, const char* lower) {
    while (*s && *lower) {
      if ((char)(*s | 0x20) != *lower) return false;
      ++s;
      ++lower;
    }
    return *s == '\0' && *lower == '\0';
  }

  // Hexadecimal, prefixo 0x opcional, 0..0x7FF.
  static bool parseId(const char* s, uint16_t& out) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (*s == '\0') return false;
    uint32_t v = 0;
    for (; *s; ++s) {
      char c = *s;
      uint8_t d;
      if (c >= '0' && c <= '9') d = (uint8_t)(c - '0');
      else if (c >= 'a' && c <= 'f') d = (uint8_t)(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') d = (uint8_t)(c - 'A' + 10);
      else return false;
      v = v * 16 + d;
      if (v > IdFilter<FN>::kMaxId) return false;
    }
    out = (uint16_t)v;
    return true;
  }

  IdFilter<FN>& filter_;
  const Stats& stats_;
  EmitFn emit_;
  char line_[kLineMax + 1];
  uint8_t len_;
  bool tooLong_;
};

}  // namespace sniffer

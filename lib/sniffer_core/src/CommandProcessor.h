#pragma once

#include <stdint.h>

#include "EngineDynamics.h"
#include "Frame.h"
#include "IdFilter.h"
#include "Text.h"
#include "VehicleSpeed.h"

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
//   t on | off | show   liga, desliga e mostra o envio do 0x208
//   t <campo> <valor>   altera um campo do 0x208 (liga o envio se desligado)
//   t period <ms>       período do envio
//   v <km/h> | off | show   substitui a velocidade do 0x388 (segue o quadro da ECU)
// (t e v só existem se o parser recebe o TxState e o SpeedState)
// IDs em hexadecimal, com ou sem prefixo 0x, de 0 a 7FF. Um comando de filtro
// que falha não altera a lista. Toda resposta começa com '#'.
template <uint8_t FN>
class CommandProcessor {
 public:
  static const uint8_t kLineMax = 48;

  CommandProcessor(IdFilter<FN>& filter, const Stats& stats, EmitFn emit, TxState* tx = nullptr,
                   SpeedState* speed = nullptr)
      : filter_(filter),
        stats_(stats),
        emit_(emit),
        tx_(tx),
        speed_(speed),
        len_(0),
        tooLong_(false) {}

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
      say(PSTR("# erro: linha muito longa"));
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
      if (c == 't' && tx_ != nullptr) return txCommand(tok + 1, (uint8_t)(n - 1));
      if (c == 'v' && speed_ != nullptr) return speedCommand(tok + 1, (uint8_t)(n - 1));
    }
    say(PSTR("# erro: comando desconhecido (use f, s, t ou v)"));
  }

  void filterCommand(char** a, uint8_t n) {
    if (n == 0) {
      say(PSTR("# erro: uso: f <id>... | f +<id> | f -<id> | f off"));
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
        say(PSTR("# erro: nao misture IDs com e sem sinal (+/-)"));
        return;
      }
      char sign = 0;
      if (hasSign) sign = *t++;
      uint16_t id;
      if (!parseId(t, id)) {
        say(PSTR("# erro: ID invalido (hexadecimal, no maximo 7FF)"));
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
    reportCounter(PSTR("# recebidos = "), stats_.received);
    reportCounter(PSTR("# exibidos = "), stats_.shown);
    reportCounter(PSTR("# perdidos = "), stats_.lost);
  }

  void reportFilter() {
    if (!filter_.active()) {
      say(PSTR("# filtro: desativado"));
      return;
    }
    char buf[10 + 4 * FN];
    char* p = text::putStrP(buf, PSTR("# filtro:"));
    for (uint8_t i = 0; i < filter_.count(); i++) {
      *p++ = ' ';
      p = text::putHex(p, filter_.at(i), 3);
    }
    *p = '\0';
    emit_(buf);
  }

  void reportCounter(const char* flashLabel, uint32_t value) {
    char buf[32];
    char* p = text::putStrP(buf, flashLabel);
    p = text::putUint(p, value);
    *p = '\0';
    emit_(buf);
  }

  void tooMany() {
    char buf[48];
    char* p = text::putStrP(buf, PSTR("# erro: capacidade excedida (maximo "));
    p = text::putUint(p, FN);
    p = text::putStrP(p, PSTR(" IDs)"));
    *p = '\0';
    emit_(buf);
  }

  // Emite um texto fixo que está na flash (PSTR).
  void say(const char* flashMsg) {
    char buf[120];
    char* p = text::putStrP(buf, flashMsg);
    *p = '\0';
    emit_(buf);
  }

  // --- comandos t (envio do 0x208) ---

  struct FieldSpec {
    const char* name;
    TxField field;
    int16_t min;
    int16_t max;
  };

  void txCommand(char** a, uint8_t n) {
    if (n == 0) {
      say(PSTR("# erro: uso: t on | off | show | period <ms> | <campo> <valor>"));
      return;
    }
    const char* sub = a[0];

    if (equalsNoCase(sub, "on")) {
      if (n != 1) return txBadArgs();
      if (!tx_->active) {
        tx_->dyn.reset();
        tx_->active = true;
      }
      reportTx();
      return;
    }
    if (equalsNoCase(sub, "off")) {
      if (n != 1) return txBadArgs();
      tx_->active = false;
      say(PSTR("# tx desligado (somente escuta)"));
      return;
    }
    if (equalsNoCase(sub, "show")) {
      if (n != 1) return txBadArgs();
      txShow();
      return;
    }
    if (equalsNoCase(sub, "period")) {
      if (n != 2) return txBadArgs();
      long ms;
      if (!parseInt(a[1], ms) || ms < 2 || ms > 5000) {
        say(PSTR("# erro: periodo invalido (2..5000 ms)"));
        return;
      }
      tx_->periodMs = (uint16_t)ms;
      char buf[32];
      char* p = text::putStrP(buf, PSTR("# tx periodo = "));
      p = text::putUint(p, tx_->periodMs);
      p = text::putStrP(p, PSTR(" ms"));
      *p = '\0';
      emit_(buf);
      return;
    }

    static const FieldSpec kFields[] = {
        {"rpm", TxField::Rpm, 0, 8191},   {"torque", TxField::Torque, -100, 408},
        {"pedal", TxField::Pedal, 0, 100}, {"cruise", TxField::Cruise, 0, 3},
        {"brake", TxField::Brake, 0, 1},   {"ac", TxField::Ac, 0, 1},
        {"diag", TxField::Diag, 0, 1},
    };
    for (uint8_t i = 0; i < sizeof(kFields) / sizeof(kFields[0]); i++) {
      if (!equalsNoCase(sub, kFields[i].name)) continue;
      if (n != 2) return txBadArgs();
      long v;
      // Valida tudo numa cópia: um valor inválido não liga o envio nem altera o quadro.
      EngineDynamics next = tx_->dyn;
      if (!tx_->active) next.reset();
      if (!parseInt(a[1], v) || !next.set(kFields[i].field, v)) {
        txBadValue(kFields[i]);
        return;
      }
      tx_->dyn = next;
      tx_->active = true;
      reportTx();
      return;
    }
    say(PSTR("# erro: campo t desconhecido (on, off, show, period, rpm, torque, pedal, cruise, brake, ac, diag)"));
  }

  void txBadArgs() { say(PSTR("# erro: argumentos invalidos para t")); }

  void txBadValue(const FieldSpec& f) {
    char buf[64];
    char* p = text::putStrP(buf, PSTR("# erro: valor invalido para "));
    p = text::putStr(p, f.name);
    p = text::putStrP(p, PSTR(" ("));
    p = text::putInt(p, f.min);
    p = text::putStrP(p, PSTR(".."));
    p = text::putInt(p, f.max);
    p = text::putStrP(p, PSTR(")"));
    *p = '\0';
    emit_(buf);
  }

  static char* putFrameHex(char* p, const EngineDynamics& d) {
    uint8_t bytes[8];
    d.encode(bytes);
    for (uint8_t i = 0; i < 8; i++) {
      if (i) *p++ = ' ';
      p = text::putHex(p, bytes[i], 2);
    }
    return p;
  }

  void reportTx() {
    char buf[80];
    char* p = text::putStrP(buf, tx_->active ? PSTR("# tx ligado, periodo ") : PSTR("# tx desligado, periodo "));
    p = text::putUint(p, tx_->periodMs);
    p = text::putStrP(p, PSTR(" ms, quadro: "));
    p = putFrameHex(p, tx_->dyn);
    *p = '\0';
    emit_(buf);
  }

  void txShow() {
    char buf[96];
    char* p = text::putStrP(buf, tx_->active ? PSTR("# tx: ligado, periodo = ") : PSTR("# tx: desligado, periodo = "));
    p = text::putUint(p, tx_->periodMs);
    p = text::putStrP(p, PSTR(" ms"));
    *p = '\0';
    emit_(buf);

    const EngineDynamics& d = tx_->dyn;
    p = text::putStrP(buf, PSTR("# tx campos: rpm="));
    p = text::putUint(p, d.rpm());
    p = text::putStrP(p, PSTR(" torque="));
    p = text::putInt(p, d.torqueNm());
    p = text::putStrP(p, PSTR(" pedal="));
    p = text::putUint(p, d.pedal());
    p = text::putStrP(p, PSTR(" cruise="));
    p = text::putUint(p, d.cruise());
    p = text::putStrP(p, PSTR(" brake="));
    p = text::putUint(p, d.brake() ? 1 : 0);
    p = text::putStrP(p, PSTR(" ac="));
    p = text::putUint(p, d.ac() ? 1 : 0);
    p = text::putStrP(p, PSTR(" diag="));
    p = text::putUint(p, d.diag() ? 1 : 0);
    *p = '\0';
    emit_(buf);

    p = text::putStrP(buf, PSTR("# tx quadro: "));
    p = putFrameHex(p, d);
    *p = '\0';
    emit_(buf);

    reportCounter(PSTR("# tx enviados = "), tx_->sent);
    reportCounter(PSTR("# tx falhas = "), tx_->failed);
  }

  // --- comandos v (substituição de velocidade do 0x388) ---

  void speedCommand(char** a, uint8_t n) {
    if (n == 0) {
      say(PSTR("# erro: uso: v <km/h> | v off | v show"));
      return;
    }
    if (n != 1) {
      say(PSTR("# erro: argumentos invalidos para v"));
      return;
    }
    if (equalsNoCase(a[0], "off")) {
      speed_->active = false;
      say(PSTR("# v desligado"));
      return;
    }
    if (equalsNoCase(a[0], "show")) {
      speedShow();
      return;
    }
    long kmh;
    if (!parseInt(a[0], kmh) || kmh < 0 || kmh > (long)kMaxKmh) {
      say(PSTR("# erro: velocidade invalida (0..300 km/h)"));
      return;
    }
    speed_->kmh = (uint16_t)kmh;
    speed_->active = true;
    char buf[64];
    char* p = text::putStrP(buf, PSTR("# v ligado, "));
    p = text::putUint(p, speed_->kmh);
    p = text::putStrP(p, PSTR(" km/h (segue o 0x388 da ECU)"));
    *p = '\0';
    emit_(buf);
  }

  void speedShow() {
    char buf[64];
    char* p = text::putStrP(buf, speed_->active ? PSTR("# v: ligado, velocidade = ")
                                                 : PSTR("# v: desligado, velocidade = "));
    p = text::putUint(p, speed_->kmh);
    p = text::putStrP(p, PSTR(" km/h"));
    *p = '\0';
    emit_(buf);
    reportCounter(PSTR("# v enviados = "), speed_->sent);
    reportCounter(PSTR("# v falhas = "), speed_->failed);
  }

  // Inteiro decimal com sinal opcional, até 6 dígitos.
  static bool parseInt(const char* s, long& out) {
    bool neg = false;
    if (*s == '-' || *s == '+') {
      neg = (*s == '-');
      ++s;
    }
    if (*s == '\0') return false;
    long v = 0;
    uint8_t digits = 0;
    for (; *s; ++s) {
      if (*s < '0' || *s > '9' || ++digits > 6) return false;
      v = v * 10 + (*s - '0');
    }
    out = neg ? -v : v;
    return true;
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
  TxState* tx_;
  SpeedState* speed_;
  char line_[kLineMax + 1];
  uint8_t len_;
  bool tooLong_;
};

}  // namespace sniffer

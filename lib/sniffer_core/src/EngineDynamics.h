#pragma once

#include <stdint.h>

namespace sniffer {

enum class TxField : uint8_t { Rpm, Torque, Pedal, Cruise, Brake, Ac, Diag };

// Conteúdo do quadro 0x208 (ENGINE_DYNAMICS) guardado em valores "crus", para
// reproduzir a referência bit a bit. Layout (bytes de 1 a 8):
//   1-2  rotação, big-endian, rpm x 8
//   3    torque real,                  Nm = cru x 2 - 100
//   4    demanda do condutor (pedal),  % = cru / 2
//   5    bit 6 DIAG_MUX_ON, bits 3-2 estado do cruise,
//        bit 1 contato secundário do freio, bit 0 pedido do A/C
//   6, 7, 8  torques (mesma escala do byte 3)
// Escalas das definições do PSA-RE para AEE2004/AEE2010, coerentes com as
// capturas feitas no carro; não confirmadas para o AEE2001.
class EngineDynamics {
 public:
  static const uint16_t kId = 0x208;

  EngineDynamics() { reset(); }

  // Quadro de referência capturado com o motor em marcha lenta:
  // 1F 58 2D 00 40 2D 2D 2D (1003 rpm, -10 Nm, pedal 0%, cruise 0, DIAG 1).
  void reset() {
    rpmRaw_ = 8024;
    torqueRaw_ = 45;
    demandRaw_ = 0;
    cruise_ = 0;
    brake_ = false;
    ac_ = false;
    diag_ = true;
  }

  // Cada setter devolve false, sem alterar nada, se o valor está fora da faixa.
  bool setRpm(long rpm) {
    if (rpm < 0 || rpm > 8191) return false;
    rpmRaw_ = (uint16_t)(rpm * 8);
    return true;
  }
  bool setTorque(long nm) {
    if (nm < -100 || nm > 408) return false;
    torqueRaw_ = (uint8_t)((nm + 100 + 1) / 2);  // resolução de 2 Nm, arredonda para cima
    return true;
  }
  bool setPedal(long percent) {
    if (percent < 0 || percent > 100) return false;
    demandRaw_ = (uint8_t)(percent * 2);
    return true;
  }
  bool setCruise(long state) {
    if (state < 0 || state > 3) return false;
    cruise_ = (uint8_t)state;
    return true;
  }
  bool setBrake(long on) { return setFlag(brake_, on); }
  bool setAc(long on) { return setFlag(ac_, on); }
  bool setDiag(long on) { return setFlag(diag_, on); }

  bool set(TxField field, long value) {
    switch (field) {
      case TxField::Rpm: return setRpm(value);
      case TxField::Torque: return setTorque(value);
      case TxField::Pedal: return setPedal(value);
      case TxField::Cruise: return setCruise(value);
      case TxField::Brake: return setBrake(value);
      case TxField::Ac: return setAc(value);
      case TxField::Diag: return setDiag(value);
    }
    return false;
  }

  uint16_t rpm() const { return rpmRaw_ / 8; }
  int16_t torqueNm() const { return (int16_t)((int16_t)torqueRaw_ * 2 - 100); }
  uint8_t pedal() const { return demandRaw_ / 2; }
  uint8_t cruise() const { return cruise_; }
  bool brake() const { return brake_; }
  bool ac() const { return ac_; }
  bool diag() const { return diag_; }

  void encode(uint8_t out[8]) const {
    out[0] = (uint8_t)(rpmRaw_ >> 8);
    out[1] = (uint8_t)(rpmRaw_ & 0xFF);
    out[2] = torqueRaw_;
    out[3] = demandRaw_;
    out[4] = (uint8_t)((diag_ ? 0x40 : 0) | (cruise_ << 2) | (brake_ ? 0x02 : 0) | (ac_ ? 0x01 : 0));
    out[5] = torqueRaw_;
    out[6] = torqueRaw_;
    out[7] = torqueRaw_;
  }

 private:
  static bool setFlag(bool& flag, long on) {
    if (on != 0 && on != 1) return false;
    flag = (on == 1);
    return true;
  }

  uint16_t rpmRaw_;
  uint8_t torqueRaw_;
  uint8_t demandRaw_;
  uint8_t cruise_;
  bool brake_;
  bool ac_;
  bool diag_;
};

// Estado do envio, compartilhado entre o parser de comandos (que altera) e o
// laço principal (que reage a `active` e envia).
struct TxState {
  EngineDynamics dyn;
  bool active;
  uint16_t periodMs;
  uint32_t sent;    // quadros enfileirados no controlador
  uint32_t failed;  // tentativas sem buffer de envio livre

  TxState() : active(false), periodMs(10), sent(0), failed(0) {}
};

}  // namespace sniffer

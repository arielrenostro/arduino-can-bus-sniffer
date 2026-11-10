// CAN sniffer do HS.IS do Peugeot 206 (MCP2515): captura passiva por padrão e,
// sob comando, envio do quadro 0x208 (ENGINE_DYNAMICS).
// Ver openspec/specs e openspec/changes para requisitos e decisões.

#include <Arduino.h>
#include <mcp2515.h>

#include "CommandProcessor.h"
#include "EngineDynamics.h"
#include "FrameRing.h"
#include "IdFilter.h"
#include "LineFormat.h"
#include "VehicleSpeed.h"
#include "config.h"

using namespace sniffer;

typedef IdFilter<CFG_FILTER_CAPACITY> Filter;

static MCP2515 mcp(CFG_PIN_CS);
static FrameRing<CFG_RING_CAPACITY> ring;
static Filter filter;
static Stats stats;
static TxState tx;
static SpeedState speed;
static uint32_t controllerOverflows = 0;  // estouros dos buffers do MCP2515
static bool captureOn = false;
static uint8_t initError = 0;
static uint32_t lastInitErrorMs = 0;
static bool txRunning = false;  // controlador em modo normal, enviando o 0x208
static uint32_t nextSendUs = 0;

// Respostas de comando e mensagens do sistema. Sempre começam com '#'.
static void emitLine(const char* text) {
  Serial.print(text);
  Serial.write('\n');
}

// Igual a emitLine, para um texto fixo na flash (PSTR).
static void emitP(const char* flashText) {
  char buf[104];
  char* p = text::putStrP(buf, flashText);
  *p = '\0';
  emitLine(buf);
}

static CommandProcessor<CFG_FILTER_CAPACITY> commands(filter, stats, emitLine, &tx, &speed);

static void syncLost() { stats.lost = ring.lost() + controllerOverflows; }

// --- Inicialização -----------------------------------------------------

static void reportInitError() {
  char buf[104];
  char* p = text::putStrP(buf, PSTR("# erro: falha ao inicializar o MCP2515 (codigo "));
  p = text::putUint(p, initError);
  p = text::putStrP(p, PSTR("); verifique fiacao, cristal e velocidade"));
  *p = '\0';
  emitLine(buf);
}

static void initCapture() {
  MCP2515::ERROR e = mcp.reset();
  if (e == MCP2515::ERROR_OK) e = mcp.setBitrate(CFG_CAN_SPEED, CFG_CAN_CLOCK);
  // Somente escuta: não transmite quadros, nem ACK, nem quadros de erro.
  if (e == MCP2515::ERROR_OK) e = mcp.setListenOnlyMode();

  if (e != MCP2515::ERROR_OK) {
    initError = (uint8_t)e;
    reportInitError();
    lastInitErrorMs = millis();
    return;
  }

  char buf[140];
  char* p = text::putStrP(buf, PSTR("# can-sniffer: captura a "));
  p = text::putUint(p, CFG_CAN_SPEED_KBPS);
  p = text::putStrP(p, PSTR(" kbit/s, cristal "));
  p = text::putUint(p, CFG_CAN_CLOCK_MHZ);
  p = text::putStrP(p, PSTR(" MHz, somente escuta (f = filtro, s = estado, t = enviar 0x208, v = velocidade)"));
  *p = '\0';
  emitLine(buf);
  captureOn = true;
}

// --- Recepção ----------------------------------------------------------

// Substituição de velocidade: a cada 0x388 da ECU, envia logo em seguida uma
// cópia com só a velocidade trocada. Sem temporizador próprio, o quadro sai
// depois do da ECU e os dois nunca disputam o barramento.
static void echoSpeed(const can_frame& cf) {
  uint8_t data[8];
  if (!buildSpeedFrame(cf.data, cf.can_dlc, speed.kmh, data)) return;

  can_frame f;
  f.can_id = kSpeedFrameId;  // identificador padrão de 11 bits
  f.can_dlc = cf.can_dlc;
  for (uint8_t i = 0; i < cf.can_dlc; i++) f.data[i] = data[i];
  if (mcp.sendMessage(&f) == MCP2515::ERROR_OK) {
    speed.sent++;
  } else {
    speed.failed++;  // todos os buffers de envio do controlador ocupados
  }
}

static void onFrame(const can_frame& cf, uint32_t now) {
  stats.received++;

  // Fora do escopo: quadros estendidos e remotos só são contados.
  if (cf.can_id & (CAN_EFF_FLAG | CAN_RTR_FLAG)) return;

  const uint16_t id = (uint16_t)cf.can_id;
  if (id == kSpeedFrameId && speed.active && txRunning) echoSpeed(cf);

  if (!filter.allows(id)) return;  // não ocupa espaço no ring

  Frame f;
  f.ms = now;
  f.id = id;
  f.len = cf.can_dlc;
  for (uint8_t i = 0; i < 8; i++) f.data[i] = (i < cf.can_dlc) ? cf.data[i] : 0;

  if (!ring.push(f)) syncLost();
}

// Quadro pendente é sinalizado pelo INT em nível baixo. O nível (e não a borda)
// permanece baixo até todos os buffers serem lidos, então nenhum evento se perde.
static void drainController() {
  for (uint8_t n = 0; n < 4 && digitalRead(CFG_PIN_INT) == LOW; n++) {
    can_frame cf;
    const MCP2515::ERROR r = mcp.readMessage(&cf);

    if (r == MCP2515::ERROR_OK) {
      onFrame(cf, millis());
    } else if (r == MCP2515::ERROR_NOMSG) {
      // INT baixo sem quadro: flag de erro (ex.: estouro de buffer).
      const uint8_t eflg = mcp.getErrorFlags();
      if (eflg & MCP2515::EFLG_RX0OVR) controllerOverflows++;
      if (eflg & MCP2515::EFLG_RX1OVR) controllerOverflows++;
      if (eflg & (MCP2515::EFLG_RX0OVR | MCP2515::EFLG_RX1OVR)) mcp.clearRXnOVRFlags();
      mcp.clearERRIF();
      mcp.clearMERR();
      syncLost();
      return;
    } else {
      // Quadro ilegível (DLC > 8): sem isso o mesmo quadro seria relido para sempre.
      stats.received++;
      controllerOverflows++;
      mcp.clearInterrupts();
      syncLost();
      return;
    }
  }
}

// --- Envio do 0x208 ----------------------------------------------------

// O controlador só transmite fora do modo somente escuta. Segue os comandos t
// e v: liga o modo normal quando qualquer um dos dois envios começa e volta a
// somente escuta quando ambos estão desligados.
static void updateTxMode() {
  const bool want = tx.active || speed.active;
  if (want == txRunning) return;

  if (want) {
    if (mcp.setNormalMode() == MCP2515::ERROR_OK) {
      txRunning = true;
    } else {
      tx.active = false;
      speed.active = false;
      mcp.setListenOnlyMode();
      emitP(PSTR("# erro: nao foi possivel entrar em modo normal; envio desligado"));
    }
  } else {
    if (mcp.setListenOnlyMode() != MCP2515::ERROR_OK) {
      emitP(PSTR("# erro: falha ao voltar ao modo somente escuta"));
    }
    txRunning = false;
  }
}

// Enfileira o 0x208 a cada periodMs. O horário é acumulado (sem deriva); se o
// laço atrasar mais que um período, ressincroniza em vez de enviar em rajada.
static void sendIfDue() {
  static bool wasActive = false;
  if (!txRunning || !tx.active) {
    wasActive = false;
    return;
  }
  const uint32_t now = micros();
  if (!wasActive) {  // acabou de ligar: o primeiro quadro sai já
    wasActive = true;
    nextSendUs = now;
  }
  if ((int32_t)(now - nextSendUs) < 0) return;

  const uint32_t period = (uint32_t)tx.periodMs * 1000UL;
  nextSendUs += period;
  if ((int32_t)(now - nextSendUs) >= 0) nextSendUs = now + period;

  can_frame f;
  f.can_id = EngineDynamics::kId;  // identificador padrão de 11 bits
  f.can_dlc = 8;
  tx.dyn.encode(f.data);
  if (mcp.sendMessage(&f) == MCP2515::ERROR_OK) {
    tx.sent++;
  } else {
    tx.failed++;  // todos os buffers de envio do controlador ocupados
  }
}

// --- Saída -------------------------------------------------------------

// Emite no máximo um quadro por chamada e só se a linha inteira couber no
// buffer de TX: assim Serial.write nunca bloqueia e a recepção não é atrasada.
static void printPending() {
  const Frame* f;
  while ((f = ring.peek()) != nullptr) {
    if (!filter.allows(f->id)) {  // filtro mudou depois de o quadro entrar
      ring.drop();
      continue;
    }
    if (Serial.availableForWrite() < (int)(kMaxFrameLine - 1)) return;

    char buf[kMaxFrameLine];
    const uint8_t n = formatFrame(*f, buf);
    Serial.write((const uint8_t*)buf, n);
    stats.shown++;
    ring.drop();
    return;
  }
}

static void readCommands() {
  if (Serial.available() > 0) commands.feed((char)Serial.read());
}

void setup() {
  Serial.begin(CFG_SERIAL_BAUD);
  pinMode(CFG_PIN_INT, INPUT_PULLUP);  // INT é ativo em nível baixo
  initCapture();
}

void loop() {
  if (captureOn) {
    updateTxMode();
    sendIfDue();
    drainController();
    printPending();
  } else {
    if (tx.active || speed.active) {  // sem controlador inicializado não há como enviar
      tx.active = false;
      speed.active = false;
      emitP(PSTR("# erro: MCP2515 nao inicializado; envio desligado"));
    }
    if (millis() - lastInitErrorMs >= 5000) {
      reportInitError();  // repete, para quem abrir o monitor depois
      lastInitErrorMs = millis();
    }
  }
  readCommands();
}

#include <string.h>
#include <unity.h>

#include "CommandProcessor.h"

using namespace sniffer;

typedef IdFilter<16> Filter;
typedef CommandProcessor<16> Processor;

static char g_lines[24][120];
static int g_n;

static void capture(const char* text) {
  if (g_n < 24) {
    strncpy(g_lines[g_n], text, 119);
    g_lines[g_n][119] = '\0';
  }
  g_n++;
}

static Filter g_filter;
static Stats g_stats;
static TxState g_tx;

void setUp() {
  g_n = 0;
  g_filter = Filter();
  g_stats = Stats();
  g_tx = TxState();
}
void tearDown() {}

static void feed(Processor& p, const char* s) {
  for (; *s; ++s) p.feed(*s);
}

#define NEW_PROC Processor proc(g_filter, g_stats, capture, &g_tx)

static void expectFrame(const uint8_t expected[8]) {
  uint8_t out[8];
  g_tx.dyn.encode(out);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, 8);
}

static const uint8_t kRef[8] = {0x1F, 0x58, 0x2D, 0x00, 0x40, 0x2D, 0x2D, 0x2D};

// ---- Envio desligado por padrão ----
void test_starts_off() {
  NEW_PROC;
  TEST_ASSERT_FALSE(g_tx.active);
}

// ---- Quadro de referência ----
void test_t_on_starts_with_reference_frame() {
  NEW_PROC;
  feed(proc, "t on\n");
  TEST_ASSERT_TRUE(g_tx.active);
  expectFrame(kRef);
  TEST_ASSERT_EQUAL(1, g_n);
  TEST_ASSERT_EQUAL_STRING("# tx ligado, periodo 10 ms, quadro: 1F 58 2D 00 40 2D 2D 2D", g_lines[0]);
}

void test_first_field_command_turns_on_with_reference_plus_change() {
  NEW_PROC;
  feed(proc, "t cruise 2\n");
  TEST_ASSERT_TRUE(g_tx.active);
  const uint8_t exp[8] = {0x1F, 0x58, 0x2D, 0x00, 0x48, 0x2D, 0x2D, 0x2D};
  expectFrame(exp);
  TEST_ASSERT_EQUAL_STRING("# tx ligado, periodo 10 ms, quadro: 1F 58 2D 00 48 2D 2D 2D", g_lines[0]);
}

void test_restart_goes_back_to_reference() {
  NEW_PROC;
  feed(proc, "t cruise 2\nt off\nt pedal 10\n");
  TEST_ASSERT_TRUE(g_tx.active);
  const uint8_t exp[8] = {0x1F, 0x58, 0x2D, 0x14, 0x40, 0x2D, 0x2D, 0x2D};  // cruise voltou a 0
  expectFrame(exp);
}

void test_t_on_while_on_does_not_reset_the_fields() {
  NEW_PROC;
  feed(proc, "t cruise 2\nt on\n");
  TEST_ASSERT_EQUAL_UINT8(2, g_tx.dyn.cruise());
  TEST_ASSERT_TRUE(g_tx.active);
}

// ---- Campos e conversões ----
void test_rpm() {
  NEW_PROC;
  feed(proc, "t rpm 2000\n");
  uint8_t out[8];
  g_tx.dyn.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x3E, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0x80, out[1]);
}

void test_torque_110_gives_0x69() {
  NEW_PROC;
  feed(proc, "t torque 110\n");
  uint8_t out[8];
  g_tx.dyn.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x69, out[2]);
  TEST_ASSERT_EQUAL_HEX8(0x69, out[7]);
}

void test_negative_torque_is_accepted() {
  NEW_PROC;
  feed(proc, "t torque -10\n");
  TEST_ASSERT_EQUAL_INT16(-10, g_tx.dyn.torqueNm());
}

void test_pedal_and_cruise_together() {
  NEW_PROC;
  feed(proc, "t pedal 50\nt cruise 3\n");
  uint8_t out[8];
  g_tx.dyn.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x64, out[3]);
  TEST_ASSERT_EQUAL_HEX8(0x4C, out[4]);
}

void test_brake_ac_diag() {
  NEW_PROC;
  feed(proc, "t brake 1\nt ac 1\nt diag 0\n");
  uint8_t out[8];
  g_tx.dyn.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x03, out[4]);
}

void test_command_and_field_names_are_case_insensitive() {
  NEW_PROC;
  feed(proc, "T CRUISE 2\n");
  TEST_ASSERT_TRUE(g_tx.active);
  TEST_ASSERT_EQUAL_UINT8(2, g_tx.dyn.cruise());
}

// ---- Alteração com o envio ligado ----
void test_fields_accumulate_while_on() {
  NEW_PROC;
  feed(proc, "t cruise 2\nt rpm 1500\n");
  TEST_ASSERT_EQUAL_UINT8(2, g_tx.dyn.cruise());
  TEST_ASSERT_EQUAL_UINT16(1500, g_tx.dyn.rpm());
  uint8_t out[8];
  g_tx.dyn.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x2E, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0xE0, out[1]);
}

// ---- Desligar ----
void test_t_off_stops_sending() {
  NEW_PROC;
  feed(proc, "t on\nt off\n");
  TEST_ASSERT_FALSE(g_tx.active);
  TEST_ASSERT_EQUAL_STRING("# tx desligado (somente escuta)", g_lines[1]);
}

// ---- Período ----
void test_period_changes_the_interval() {
  NEW_PROC;
  feed(proc, "t on\nt period 100\n");
  TEST_ASSERT_EQUAL_UINT16(100, g_tx.periodMs);
  TEST_ASSERT_TRUE(g_tx.active);
  TEST_ASSERT_EQUAL_STRING("# tx periodo = 100 ms", g_lines[1]);
}

void test_period_does_not_turn_sending_on_and_persists_after_off() {
  NEW_PROC;
  feed(proc, "t period 50\n");
  TEST_ASSERT_FALSE(g_tx.active);
  feed(proc, "t cruise 1\nt off\nt on\n");
  TEST_ASSERT_EQUAL_UINT16(50, g_tx.periodMs);
}

void test_invalid_period_is_rejected() {
  NEW_PROC;
  feed(proc, "t period 1\nt period 5001\nt period abc\nt period\n");
  TEST_ASSERT_EQUAL_UINT16(10, g_tx.periodMs);
  TEST_ASSERT_EQUAL_STRING("# erro: periodo invalido (2..5000 ms)", g_lines[0]);
  TEST_ASSERT_EQUAL_STRING("# erro: periodo invalido (2..5000 ms)", g_lines[1]);
  TEST_ASSERT_EQUAL_STRING("# erro: periodo invalido (2..5000 ms)", g_lines[2]);
  TEST_ASSERT_EQUAL_STRING("# erro: argumentos invalidos para t", g_lines[3]);
}

void test_period_limits_are_accepted() {
  NEW_PROC;
  feed(proc, "t period 2\n");
  TEST_ASSERT_EQUAL_UINT16(2, g_tx.periodMs);
  feed(proc, "t period 5000\n");
  TEST_ASSERT_EQUAL_UINT16(5000, g_tx.periodMs);
}

// ---- Rejeição de comandos inválidos ----
void test_out_of_range_value_does_not_turn_sending_on() {
  NEW_PROC;
  feed(proc, "t cruise 4\n");
  TEST_ASSERT_FALSE(g_tx.active);
  expectFrame(kRef);
  TEST_ASSERT_EQUAL_STRING("# erro: valor invalido para cruise (0..3)", g_lines[0]);
}

void test_non_numeric_value_keeps_the_frame() {
  NEW_PROC;
  feed(proc, "t cruise 2\nt rpm abc\n");
  TEST_ASSERT_EQUAL_UINT16(1003, g_tx.dyn.rpm());
  TEST_ASSERT_EQUAL_UINT8(2, g_tx.dyn.cruise());
  TEST_ASSERT_EQUAL_STRING("# erro: valor invalido para rpm (0..8191)", g_lines[1]);
}

void test_non_integer_and_garbage_values_are_rejected() {
  NEW_PROC;
  feed(proc, "t rpm 1.5\nt rpm 12x\nt rpm -\nt rpm 9999999\n");
  TEST_ASSERT_FALSE(g_tx.active);
  TEST_ASSERT_EQUAL(4, g_n);
  for (int i = 0; i < 4; i++) TEST_ASSERT_EQUAL_STRING_LEN("# erro: valor invalido para rpm", g_lines[i], 31);
}

void test_torque_below_minimum_is_rejected_with_range_in_message() {
  NEW_PROC;
  feed(proc, "t torque -101\n");
  TEST_ASSERT_FALSE(g_tx.active);
  TEST_ASSERT_EQUAL_STRING("# erro: valor invalido para torque (-100..408)", g_lines[0]);
}

void test_missing_or_extra_arguments_are_rejected() {
  NEW_PROC;
  feed(proc, "t rpm\nt rpm 1 2\nt on 5\nt off now\nt show x\n");
  TEST_ASSERT_FALSE(g_tx.active);
  TEST_ASSERT_EQUAL(5, g_n);
  for (int i = 0; i < 5; i++) TEST_ASSERT_EQUAL_STRING("# erro: argumentos invalidos para t", g_lines[i]);
}

void test_unknown_field_and_bare_t() {
  NEW_PROC;
  feed(proc, "t foo 1\nt\n");
  TEST_ASSERT_FALSE(g_tx.active);
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: campo t desconhecido", g_lines[0], 28);
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: uso: t", g_lines[1], 14);
}

// ---- Estado ----
void test_show_when_on() {
  NEW_PROC;
  g_tx.sent = 1234;
  g_tx.failed = 2;
  feed(proc, "t cruise 2\nt show\n");
  TEST_ASSERT_EQUAL(1 + 5, g_n);
  TEST_ASSERT_EQUAL_STRING("# tx: ligado, periodo = 10 ms", g_lines[1]);
  TEST_ASSERT_EQUAL_STRING("# tx campos: rpm=1003 torque=-10 pedal=0 cruise=2 brake=0 ac=0 diag=1", g_lines[2]);
  TEST_ASSERT_EQUAL_STRING("# tx quadro: 1F 58 2D 00 48 2D 2D 2D", g_lines[3]);
  TEST_ASSERT_EQUAL_STRING("# tx enviados = 1234", g_lines[4]);
  TEST_ASSERT_EQUAL_STRING("# tx falhas = 2", g_lines[5]);
}

void test_show_when_off() {
  NEW_PROC;
  feed(proc, "t show\n");
  TEST_ASSERT_EQUAL_STRING("# tx: desligado, periodo = 10 ms", g_lines[0]);
  TEST_ASSERT_EQUAL_STRING("# tx quadro: 1F 58 2D 00 40 2D 2D 2D", g_lines[2]);
}

void test_show_does_not_change_anything() {
  NEW_PROC;
  feed(proc, "t show\n");
  TEST_ASSERT_FALSE(g_tx.active);
  expectFrame(kRef);
}

// ---- Sem TxState, t não existe; f e s continuam funcionando ----
void test_t_is_unknown_without_a_tx_state() {
  Processor proc(g_filter, g_stats, capture);
  feed(proc, "t on\nf 412\n");
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: comando desconhecido", g_lines[0], 28);
  TEST_ASSERT_TRUE(g_filter.allows(0x412));
  TEST_ASSERT_FALSE(g_tx.active);
}

void test_f_and_s_still_work_with_a_tx_state() {
  NEW_PROC;
  feed(proc, "f 412\ns\n");
  TEST_ASSERT_TRUE(g_filter.active());
  TEST_ASSERT_EQUAL_STRING("# filtro: 412", g_lines[0]);
}

void test_every_response_starts_with_hash() {
  NEW_PROC;
  feed(proc, "t on\nt cruise 2\nt show\nt rpm x\nt period 100\nt off\nt\nt foo\n");
  TEST_ASSERT_TRUE(g_n > 8);
  for (int i = 0; i < g_n && i < 24; i++) TEST_ASSERT_EQUAL_CHAR('#', g_lines[i][0]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_off);
  RUN_TEST(test_t_on_starts_with_reference_frame);
  RUN_TEST(test_first_field_command_turns_on_with_reference_plus_change);
  RUN_TEST(test_restart_goes_back_to_reference);
  RUN_TEST(test_t_on_while_on_does_not_reset_the_fields);
  RUN_TEST(test_rpm);
  RUN_TEST(test_torque_110_gives_0x69);
  RUN_TEST(test_negative_torque_is_accepted);
  RUN_TEST(test_pedal_and_cruise_together);
  RUN_TEST(test_brake_ac_diag);
  RUN_TEST(test_command_and_field_names_are_case_insensitive);
  RUN_TEST(test_fields_accumulate_while_on);
  RUN_TEST(test_t_off_stops_sending);
  RUN_TEST(test_period_changes_the_interval);
  RUN_TEST(test_period_does_not_turn_sending_on_and_persists_after_off);
  RUN_TEST(test_invalid_period_is_rejected);
  RUN_TEST(test_period_limits_are_accepted);
  RUN_TEST(test_out_of_range_value_does_not_turn_sending_on);
  RUN_TEST(test_non_numeric_value_keeps_the_frame);
  RUN_TEST(test_non_integer_and_garbage_values_are_rejected);
  RUN_TEST(test_torque_below_minimum_is_rejected_with_range_in_message);
  RUN_TEST(test_missing_or_extra_arguments_are_rejected);
  RUN_TEST(test_unknown_field_and_bare_t);
  RUN_TEST(test_show_when_on);
  RUN_TEST(test_show_when_off);
  RUN_TEST(test_show_does_not_change_anything);
  RUN_TEST(test_t_is_unknown_without_a_tx_state);
  RUN_TEST(test_f_and_s_still_work_with_a_tx_state);
  RUN_TEST(test_every_response_starts_with_hash);
  return UNITY_END();
}

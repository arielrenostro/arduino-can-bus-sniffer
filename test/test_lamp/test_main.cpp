#include <string.h>
#include <unity.h>

#include "CommandProcessor.h"
#include "LampRequest.h"

using namespace sniffer;

void setUp() {}
void tearDown() {}

// Quadro 0x488 capturado da ECU do usuário.
static const uint8_t kEcu[8] = {0x43, 0x9A, 0xFF, 0x04, 0x00, 0x43, 0x1A, 0x45};

// ================= Montagem do quadro (task 1.1) =================

void test_preheat_1_sets_bit1_and_keeps_the_mil_bit() {
  uint8_t out[8];
  TEST_ASSERT_TRUE(buildLampFrame(kEcu, 8, 1, -1, out));
  const uint8_t exp[8] = {0x43, 0x9A, 0xFF, 0x06, 0x00, 0x43, 0x1A, 0x45};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(exp, out, 8);
}

void test_mil_0_clears_the_mil_bit() {
  uint8_t out[8];
  TEST_ASSERT_TRUE(buildLampFrame(kEcu, 8, -1, 0, out));
  const uint8_t exp[8] = {0x43, 0x9A, 0xFF, 0x00, 0x00, 0x43, 0x1A, 0x45};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(exp, out, 8);
}

void test_both_fields() {
  uint8_t out[8];
  buildLampFrame(kEcu, 8, 1, 0, out);
  TEST_ASSERT_EQUAL_HEX8(0x02, out[3]);
  buildLampFrame(kEcu, 8, 1, 1, out);
  TEST_ASSERT_EQUAL_HEX8(0x06, out[3]);
  buildLampFrame(kEcu, 8, 0, 0, out);
  TEST_ASSERT_EQUAL_HEX8(0x00, out[3]);
}

void test_following_the_ecu_changes_nothing() {
  uint8_t out[8];
  buildLampFrame(kEcu, 8, -1, -1, out);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(kEcu, out, 8);
}

void test_other_bits_of_byte4_are_preserved() {
  const uint8_t src[8] = {0, 0, 0, 0xFD, 0, 0, 0, 0};  // tudo ligado, menos o bit 1
  uint8_t out[8];
  buildLampFrame(src, 8, 1, 0, out);
  TEST_ASSERT_EQUAL_HEX8(0xFB, out[3]);  // bit 1 ligado, bit 2 desligado, o resto igual
}

void test_length_is_kept_and_short_frames_refused() {
  uint8_t out[8];
  TEST_ASSERT_TRUE(buildLampFrame(kEcu, 4, 1, -1, out));
  TEST_ASSERT_FALSE(buildLampFrame(kEcu, 3, 1, -1, out));
  TEST_ASSERT_FALSE(buildLampFrame(kEcu, 0, 1, -1, out));
  TEST_ASSERT_FALSE(buildLampFrame(kEcu, 9, 1, -1, out));
}

void test_lamp_state_defaults_and_clear() {
  LampState s;
  TEST_ASSERT_FALSE(s.active);
  TEST_ASSERT_EQUAL_INT8(-1, s.preheat);
  TEST_ASSERT_EQUAL_INT8(-1, s.mil);
  s.active = true;
  s.preheat = 1;
  s.mil = 0;
  s.clear();
  TEST_ASSERT_FALSE(s.active);
  TEST_ASSERT_EQUAL_INT8(-1, s.preheat);
  TEST_ASSERT_EQUAL_INT8(-1, s.mil);
}

// ================= Comandos l (task 2.1) =================

typedef IdFilter<16> Filter;
typedef CommandProcessor<16> Processor;

static char g_lines[16][120];
static int g_n;
static void capture(const char* text) {
  if (g_n < 16) {
    strncpy(g_lines[g_n], text, 119);
    g_lines[g_n][119] = '\0';
  }
  g_n++;
}
static Filter g_filter;
static Stats g_stats;
static TxState g_tx;
static SpeedState g_speed;
static LampState g_lamp;

static void reset() {
  g_n = 0;
  g_filter = Filter();
  g_stats = Stats();
  g_tx = TxState();
  g_speed = SpeedState();
  g_lamp = LampState();
}
static void feed(Processor& p, const char* s) {
  for (; *s; ++s) p.feed(*s);
}
#define NEW_PROC \
  reset();       \
  Processor proc(g_filter, g_stats, capture, &g_tx, &g_speed, &g_lamp)

void test_l_preheat_1_turns_on_and_keeps_the_mil_following_the_ecu() {
  NEW_PROC;
  feed(proc, "l preheat 1\n");
  TEST_ASSERT_TRUE(g_lamp.active);
  TEST_ASSERT_EQUAL_INT8(1, g_lamp.preheat);
  TEST_ASSERT_EQUAL_INT8(-1, g_lamp.mil);
  TEST_ASSERT_EQUAL(1, g_n);
  TEST_ASSERT_EQUAL_STRING("# l ligado: preheat=1 mil=ecu (segue o 0x488 da ECU)", g_lines[0]);
}

void test_l_mil_0_turns_on() {
  NEW_PROC;
  feed(proc, "l mil 0\n");
  TEST_ASSERT_TRUE(g_lamp.active);
  TEST_ASSERT_EQUAL_INT8(0, g_lamp.mil);
  TEST_ASSERT_EQUAL_INT8(-1, g_lamp.preheat);
  TEST_ASSERT_EQUAL_STRING("# l ligado: preheat=ecu mil=0 (segue o 0x488 da ECU)", g_lines[0]);
}

void test_both_fields_accumulate() {
  NEW_PROC;
  feed(proc, "l preheat 1\nl mil 0\n");
  TEST_ASSERT_EQUAL_INT8(1, g_lamp.preheat);
  TEST_ASSERT_EQUAL_INT8(0, g_lamp.mil);
  TEST_ASSERT_EQUAL_STRING("# l ligado: preheat=1 mil=0 (segue o 0x488 da ECU)", g_lines[1]);
}

void test_l_off_returns_the_fields_to_the_ecu() {
  NEW_PROC;
  feed(proc, "l preheat 1\nl off\n");
  TEST_ASSERT_FALSE(g_lamp.active);
  TEST_ASSERT_EQUAL_INT8(-1, g_lamp.preheat);
  TEST_ASSERT_EQUAL_STRING("# l desligado", g_lines[1]);
  feed(proc, "l mil 0\n");  // religar parte do zero
  TEST_ASSERT_EQUAL_INT8(-1, g_lamp.preheat);
  TEST_ASSERT_EQUAL_INT8(0, g_lamp.mil);
}

void test_invalid_value_does_not_turn_on() {
  NEW_PROC;
  feed(proc, "l mil 2\n");
  TEST_ASSERT_FALSE(g_lamp.active);
  TEST_ASSERT_EQUAL_STRING("# erro: valor invalido para mil (0..1)", g_lines[0]);
}

void test_invalid_values_keep_the_fields() {
  NEW_PROC;
  feed(proc, "l preheat 1\nl preheat abc\nl preheat -1\nl mil 1.5\n");
  TEST_ASSERT_TRUE(g_lamp.active);
  TEST_ASSERT_EQUAL_INT8(1, g_lamp.preheat);
  TEST_ASSERT_EQUAL_INT8(-1, g_lamp.mil);
  TEST_ASSERT_EQUAL(4, g_n);
  for (int i = 1; i < 4; i++) TEST_ASSERT_EQUAL_STRING_LEN("# erro: valor invalido para ", g_lines[i], 28);
}

void test_missing_extra_and_unknown() {
  NEW_PROC;
  feed(proc, "l\nl mil\nl mil 0 1\nl off now\nl foo 1\n");
  TEST_ASSERT_FALSE(g_lamp.active);
  TEST_ASSERT_EQUAL(5, g_n);
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: uso: l", g_lines[0], 14);
  TEST_ASSERT_EQUAL_STRING("# erro: argumentos invalidos para l", g_lines[1]);
  TEST_ASSERT_EQUAL_STRING("# erro: argumentos invalidos para l", g_lines[2]);
  TEST_ASSERT_EQUAL_STRING("# erro: argumentos invalidos para l", g_lines[3]);
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: campo l desconhecido", g_lines[4], 28);
}

void test_l_show() {
  NEW_PROC;
  g_lamp.sent = 40;
  g_lamp.failed = 0;
  feed(proc, "l preheat 1\nl show\n");
  TEST_ASSERT_EQUAL(1 + 3, g_n);
  TEST_ASSERT_EQUAL_STRING("# l: ligado, preheat=1 mil=ecu", g_lines[1]);
  TEST_ASSERT_EQUAL_STRING("# l enviados = 40", g_lines[2]);
  TEST_ASSERT_EQUAL_STRING("# l falhas = 0", g_lines[3]);
}

void test_l_show_when_off_does_not_turn_on() {
  NEW_PROC;
  feed(proc, "l show\n");
  TEST_ASSERT_FALSE(g_lamp.active);
  TEST_ASSERT_EQUAL_STRING("# l: desligado, preheat=ecu mil=ecu", g_lines[0]);
}

void test_l_is_unknown_without_a_lamp_state() {
  reset();
  Processor proc(g_filter, g_stats, capture, &g_tx, &g_speed);  // sem LampState
  feed(proc, "l mil 0\n");
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: comando desconhecido", g_lines[0], 28);
  TEST_ASSERT_FALSE(g_lamp.active);
}

void test_other_commands_still_work() {
  NEW_PROC;
  feed(proc, "t on\nv 60\nf 412\nl mil 0\n");
  TEST_ASSERT_TRUE(g_tx.active);
  TEST_ASSERT_TRUE(g_speed.active);
  TEST_ASSERT_TRUE(g_filter.active());
  TEST_ASSERT_TRUE(g_lamp.active);
}

void test_every_l_response_starts_with_hash() {
  NEW_PROC;
  feed(proc, "l preheat 1\nl show\nl mil 9\nl\nl off\nl a b c\n");
  TEST_ASSERT_TRUE(g_n >= 8);
  for (int i = 0; i < g_n && i < 16; i++) TEST_ASSERT_EQUAL_CHAR('#', g_lines[i][0]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_preheat_1_sets_bit1_and_keeps_the_mil_bit);
  RUN_TEST(test_mil_0_clears_the_mil_bit);
  RUN_TEST(test_both_fields);
  RUN_TEST(test_following_the_ecu_changes_nothing);
  RUN_TEST(test_other_bits_of_byte4_are_preserved);
  RUN_TEST(test_length_is_kept_and_short_frames_refused);
  RUN_TEST(test_lamp_state_defaults_and_clear);
  RUN_TEST(test_l_preheat_1_turns_on_and_keeps_the_mil_following_the_ecu);
  RUN_TEST(test_l_mil_0_turns_on);
  RUN_TEST(test_both_fields_accumulate);
  RUN_TEST(test_l_off_returns_the_fields_to_the_ecu);
  RUN_TEST(test_invalid_value_does_not_turn_on);
  RUN_TEST(test_invalid_values_keep_the_fields);
  RUN_TEST(test_missing_extra_and_unknown);
  RUN_TEST(test_l_show);
  RUN_TEST(test_l_show_when_off_does_not_turn_on);
  RUN_TEST(test_l_is_unknown_without_a_lamp_state);
  RUN_TEST(test_other_commands_still_work);
  RUN_TEST(test_every_l_response_starts_with_hash);
  return UNITY_END();
}

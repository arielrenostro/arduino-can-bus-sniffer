#include <string.h>
#include <unity.h>

#include "CommandProcessor.h"
#include "VehicleSpeed.h"

using namespace sniffer;

void setUp() {}
void tearDown() {}

// ================= Montagem do quadro (task 1.1) =================

void test_60_kmh_on_captured_frame() {
  const uint8_t src[5] = {0x00, 0x00, 0x00, 0x03, 0xAF};
  uint8_t out[8];
  TEST_ASSERT_TRUE(buildSpeedFrame(src, 5, 60, out));
  const uint8_t exp[5] = {0x17, 0x70, 0x00, 0x03, 0xAF};
  TEST_ASSERT_EQUAL_HEX8_ARRAY(exp, out, 5);
}

void test_other_speeds() {
  const uint8_t src[5] = {0, 0, 0, 3, 0xAF};
  uint8_t out[8];
  buildSpeedFrame(src, 5, 90, out);
  TEST_ASSERT_EQUAL_HEX8(0x23, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0x28, out[1]);
  buildSpeedFrame(src, 5, 0, out);
  TEST_ASSERT_EQUAL_HEX8(0x00, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, out[1]);
  buildSpeedFrame(src, 5, 300, out);
  TEST_ASSERT_EQUAL_HEX8(0x75, out[0]);  // 30000 = 0x7530
  TEST_ASSERT_EQUAL_HEX8(0x30, out[1]);
}

void test_distance_counter_and_acceleration_follow_the_ecu() {
  const uint8_t src[5] = {0x00, 0x00, 0x00, 0x04, 0xB0};
  uint8_t out[8];
  buildSpeedFrame(src, 5, 60, out);
  TEST_ASSERT_EQUAL_HEX8(0x00, out[2]);
  TEST_ASSERT_EQUAL_HEX8(0x04, out[3]);
  TEST_ASSERT_EQUAL_HEX8(0xB0, out[4]);
}

void test_the_speed_in_the_ecu_frame_is_replaced() {
  const uint8_t src[5] = {0x12, 0x34, 0x00, 0x03, 0xAF};
  uint8_t out[8];
  buildSpeedFrame(src, 5, 60, out);
  TEST_ASSERT_EQUAL_HEX8(0x17, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0x70, out[1]);
}

void test_length_is_kept_and_extra_bytes_copied() {
  const uint8_t src[8] = {0, 0, 1, 2, 3, 4, 5, 6};
  uint8_t out[8];
  TEST_ASSERT_TRUE(buildSpeedFrame(src, 8, 10, out));
  TEST_ASSERT_EQUAL_HEX8_ARRAY(src + 2, out + 2, 6);
  TEST_ASSERT_TRUE(buildSpeedFrame(src, 2, 10, out));
}

void test_short_and_long_frames_are_refused() {
  const uint8_t src[8] = {0};
  uint8_t out[8];
  TEST_ASSERT_FALSE(buildSpeedFrame(src, 0, 10, out));
  TEST_ASSERT_FALSE(buildSpeedFrame(src, 1, 10, out));
  TEST_ASSERT_FALSE(buildSpeedFrame(src, 9, 10, out));
}

void test_speed_state_starts_off() {
  SpeedState s;
  TEST_ASSERT_FALSE(s.active);
  TEST_ASSERT_EQUAL_UINT32(0, s.sent);
  TEST_ASSERT_EQUAL_UINT32(0, s.failed);
}

// ================= Comandos v (task 2.1) =================

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

static void reset() {
  g_n = 0;
  g_filter = Filter();
  g_stats = Stats();
  g_tx = TxState();
  g_speed = SpeedState();
}
static void feed(Processor& p, const char* s) {
  for (; *s; ++s) p.feed(*s);
}
#define NEW_PROC \
  reset();       \
  Processor proc(g_filter, g_stats, capture, &g_tx, &g_speed)

void test_v_60_turns_on() {
  NEW_PROC;
  feed(proc, "v 60\n");
  TEST_ASSERT_TRUE(g_speed.active);
  TEST_ASSERT_EQUAL_UINT16(60, g_speed.kmh);
  TEST_ASSERT_EQUAL(1, g_n);
  TEST_ASSERT_EQUAL_STRING("# v ligado, 60 km/h (segue o 0x388 da ECU)", g_lines[0]);
}

void test_v_changes_speed_while_on() {
  NEW_PROC;
  feed(proc, "v 60\nv 90\n");
  TEST_ASSERT_TRUE(g_speed.active);
  TEST_ASSERT_EQUAL_UINT16(90, g_speed.kmh);
}

void test_v_0_and_300_are_accepted() {
  NEW_PROC;
  feed(proc, "v 0\n");
  TEST_ASSERT_TRUE(g_speed.active);
  TEST_ASSERT_EQUAL_UINT16(0, g_speed.kmh);
  feed(proc, "v 300\n");
  TEST_ASSERT_EQUAL_UINT16(300, g_speed.kmh);
}

void test_v_off_stops() {
  NEW_PROC;
  feed(proc, "v 60\nv off\n");
  TEST_ASSERT_FALSE(g_speed.active);
  TEST_ASSERT_EQUAL_STRING("# v desligado", g_lines[1]);
  feed(proc, "V OFF\n");
  TEST_ASSERT_FALSE(g_speed.active);
}

void test_out_of_range_does_not_turn_on() {
  NEW_PROC;
  feed(proc, "v 301\n");
  TEST_ASSERT_FALSE(g_speed.active);
  TEST_ASSERT_EQUAL_STRING("# erro: velocidade invalida (0..300 km/h)", g_lines[0]);
}

void test_invalid_values_keep_the_speed() {
  NEW_PROC;
  feed(proc, "v 60\nv -1\nv abc\nv 1.5\nv 9999999\n");
  TEST_ASSERT_EQUAL_UINT16(60, g_speed.kmh);
  TEST_ASSERT_TRUE(g_speed.active);
  TEST_ASSERT_EQUAL(5, g_n);
  for (int i = 1; i < 5; i++)
    TEST_ASSERT_EQUAL_STRING("# erro: velocidade invalida (0..300 km/h)", g_lines[i]);
}

void test_missing_and_extra_arguments() {
  NEW_PROC;
  feed(proc, "v\nv 60 70\nv off now\n");
  TEST_ASSERT_FALSE(g_speed.active);
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: uso: v", g_lines[0], 14);
  TEST_ASSERT_EQUAL_STRING("# erro: argumentos invalidos para v", g_lines[1]);
  TEST_ASSERT_EQUAL_STRING("# erro: argumentos invalidos para v", g_lines[2]);
}

void test_v_show_when_on() {
  NEW_PROC;
  g_speed.sent = 250;
  g_speed.failed = 1;
  feed(proc, "v 60\nv show\n");
  TEST_ASSERT_EQUAL(1 + 3, g_n);
  TEST_ASSERT_EQUAL_STRING("# v: ligado, velocidade = 60 km/h", g_lines[1]);
  TEST_ASSERT_EQUAL_STRING("# v enviados = 250", g_lines[2]);
  TEST_ASSERT_EQUAL_STRING("# v falhas = 1", g_lines[3]);
}

void test_v_show_when_off_does_not_turn_on() {
  NEW_PROC;
  feed(proc, "v show\n");
  TEST_ASSERT_FALSE(g_speed.active);
  TEST_ASSERT_EQUAL_STRING("# v: desligado, velocidade = 0 km/h", g_lines[0]);
}

void test_v_is_unknown_without_a_speed_state() {
  reset();
  Processor proc(g_filter, g_stats, capture, &g_tx);  // sem SpeedState
  feed(proc, "v 60\n");
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: comando desconhecido", g_lines[0], 28);
  TEST_ASSERT_FALSE(g_speed.active);
}

void test_t_f_s_still_work_alongside_v() {
  NEW_PROC;
  feed(proc, "t on\nf 412\nv 60\n");
  TEST_ASSERT_TRUE(g_tx.active);
  TEST_ASSERT_TRUE(g_filter.active());
  TEST_ASSERT_TRUE(g_speed.active);
}

void test_every_v_response_starts_with_hash() {
  NEW_PROC;
  feed(proc, "v 60\nv show\nv 999\nv\nv off\nv 1 2\n");
  TEST_ASSERT_TRUE(g_n >= 8);
  for (int i = 0; i < g_n && i < 16; i++) TEST_ASSERT_EQUAL_CHAR('#', g_lines[i][0]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_60_kmh_on_captured_frame);
  RUN_TEST(test_other_speeds);
  RUN_TEST(test_distance_counter_and_acceleration_follow_the_ecu);
  RUN_TEST(test_the_speed_in_the_ecu_frame_is_replaced);
  RUN_TEST(test_length_is_kept_and_extra_bytes_copied);
  RUN_TEST(test_short_and_long_frames_are_refused);
  RUN_TEST(test_speed_state_starts_off);
  RUN_TEST(test_v_60_turns_on);
  RUN_TEST(test_v_changes_speed_while_on);
  RUN_TEST(test_v_0_and_300_are_accepted);
  RUN_TEST(test_v_off_stops);
  RUN_TEST(test_out_of_range_does_not_turn_on);
  RUN_TEST(test_invalid_values_keep_the_speed);
  RUN_TEST(test_missing_and_extra_arguments);
  RUN_TEST(test_v_show_when_on);
  RUN_TEST(test_v_show_when_off_does_not_turn_on);
  RUN_TEST(test_v_is_unknown_without_a_speed_state);
  RUN_TEST(test_t_f_s_still_work_alongside_v);
  RUN_TEST(test_every_v_response_starts_with_hash);
  return UNITY_END();
}

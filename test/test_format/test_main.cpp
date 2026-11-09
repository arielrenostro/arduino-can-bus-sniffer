#include <string.h>
#include <unity.h>

#include "LineFormat.h"

using namespace sniffer;

void setUp() {}
void tearDown() {}

static Frame frame(uint32_t ms, uint16_t id, uint8_t len, const uint8_t* d) {
  Frame f = {};
  f.ms = ms;
  f.id = id;
  f.len = len;
  for (uint8_t i = 0; i < len && i < 8; i++) f.data[i] = d[i];
  return f;
}

void test_frame_8_bytes_from_spec() {
  const uint8_t d[] = {0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  char out[kMaxFrameLine];
  uint8_t n = formatFrame(frame(12345, 0x412, 8, d), out);
  TEST_ASSERT_EQUAL_STRING("12345 412 [8] 00 20 00 00 00 00 00 00\n", out);
  TEST_ASSERT_EQUAL_UINT8(strlen(out), n);
}

void test_frame_without_data() {
  char out[kMaxFrameLine];
  uint8_t n = formatFrame(frame(7, 0x609, 0, nullptr), out);
  TEST_ASSERT_EQUAL_STRING("7 609 [0]\n", out);
  TEST_ASSERT_EQUAL_UINT8(10, n);
}

void test_id_is_padded_to_3_uppercase_hex_digits() {
  const uint8_t d[] = {0xAB};
  char out[kMaxFrameLine];
  formatFrame(frame(0, 0x072, 1, d), out);
  TEST_ASSERT_EQUAL_STRING("0 072 [1] AB\n", out);
  formatFrame(frame(0, 0x7E2, 1, d), out);
  TEST_ASSERT_EQUAL_STRING("0 7E2 [1] AB\n", out);
}

void test_bytes_are_padded_to_2_digits() {
  const uint8_t d[] = {0x00, 0x01, 0x0F, 0xF0, 0xFF};
  char out[kMaxFrameLine];
  formatFrame(frame(1, 0x208, 5, d), out);
  TEST_ASSERT_EQUAL_STRING("1 208 [5] 00 01 0F F0 FF\n", out);
}

void test_worst_case_line_fits_in_buffer() {
  const uint8_t d[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  char out[kMaxFrameLine];
  uint8_t n = formatFrame(frame(4294967295UL, 0x7FF, 8, d), out);
  TEST_ASSERT_EQUAL_STRING("4294967295 7FF [8] FF FF FF FF FF FF FF FF\n", out);
  TEST_ASSERT_EQUAL_UINT8(43, n);
  TEST_ASSERT_TRUE(n < kMaxFrameLine);
}

void test_ms_zero() {
  char out[kMaxFrameLine];
  formatFrame(frame(0, 0x100, 0, nullptr), out);
  TEST_ASSERT_EQUAL_STRING("0 100 [0]\n", out);
}

void test_len_above_8_is_clamped() {
  Frame f = {};
  f.id = 0x100;
  f.len = 12;
  char out[kMaxFrameLine];
  uint8_t n = formatFrame(f, out);
  TEST_ASSERT_EQUAL_STRING("0 100 [8] 00 00 00 00 00 00 00 00\n", out);
  TEST_ASSERT_TRUE(n < kMaxFrameLine);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_frame_8_bytes_from_spec);
  RUN_TEST(test_frame_without_data);
  RUN_TEST(test_id_is_padded_to_3_uppercase_hex_digits);
  RUN_TEST(test_bytes_are_padded_to_2_digits);
  RUN_TEST(test_worst_case_line_fits_in_buffer);
  RUN_TEST(test_ms_zero);
  RUN_TEST(test_len_above_8_is_clamped);
  return UNITY_END();
}

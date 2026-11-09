#include <unity.h>

#include "FrameRing.h"

using namespace sniffer;

void setUp() {}
void tearDown() {}

static Frame mk(uint16_t id) {
  Frame f = {};
  f.id = id;
  f.len = 1;
  f.data[0] = (uint8_t)id;
  return f;
}

void test_starts_empty() {
  FrameRing<4> r;
  Frame out;
  TEST_ASSERT_TRUE(r.empty());
  TEST_ASSERT_EQUAL_UINT8(0, r.size());
  TEST_ASSERT_NULL(r.peek());
  TEST_ASSERT_FALSE(r.pop(out));
  TEST_ASSERT_EQUAL_UINT32(0, r.lost());
}

void test_fifo_order() {
  FrameRing<4> r;
  Frame out;
  TEST_ASSERT_TRUE(r.push(mk(0x208)));
  TEST_ASSERT_TRUE(r.push(mk(0x412)));
  TEST_ASSERT_TRUE(r.push(mk(0x388)));
  TEST_ASSERT_EQUAL_UINT8(3, r.size());
  TEST_ASSERT_EQUAL_HEX16(0x208, r.peek()->id);
  TEST_ASSERT_TRUE(r.pop(out));
  TEST_ASSERT_EQUAL_HEX16(0x208, out.id);
  TEST_ASSERT_TRUE(r.pop(out));
  TEST_ASSERT_EQUAL_HEX16(0x412, out.id);
  TEST_ASSERT_TRUE(r.pop(out));
  TEST_ASSERT_EQUAL_HEX16(0x388, out.id);
  TEST_ASSERT_TRUE(r.empty());
}

void test_full_discards_new_frame_and_counts_lost() {
  FrameRing<4> r;
  Frame out;
  for (uint16_t i = 1; i <= 4; i++) TEST_ASSERT_TRUE(r.push(mk(i)));
  TEST_ASSERT_TRUE(r.full());
  TEST_ASSERT_EQUAL_UINT32(0, r.lost());

  TEST_ASSERT_FALSE(r.push(mk(0x7FF)));
  TEST_ASSERT_FALSE(r.push(mk(0x7FE)));
  TEST_ASSERT_EQUAL_UINT32(2, r.lost());
  TEST_ASSERT_EQUAL_UINT8(4, r.size());

  // os quadros antigos permanecem, em ordem
  for (uint16_t i = 1; i <= 4; i++) {
    TEST_ASSERT_TRUE(r.pop(out));
    TEST_ASSERT_EQUAL_HEX16(i, out.id);
  }
  TEST_ASSERT_TRUE(r.empty());
}

void test_accepts_again_after_pop() {
  FrameRing<2> r;
  Frame out;
  r.push(mk(1));
  r.push(mk(2));
  TEST_ASSERT_FALSE(r.push(mk(3)));
  TEST_ASSERT_TRUE(r.pop(out));
  TEST_ASSERT_TRUE(r.push(mk(4)));
  TEST_ASSERT_TRUE(r.pop(out));
  TEST_ASSERT_EQUAL_HEX16(2, out.id);
  TEST_ASSERT_TRUE(r.pop(out));
  TEST_ASSERT_EQUAL_HEX16(4, out.id);
}

void test_drop_removes_oldest() {
  FrameRing<4> r;
  r.push(mk(1));
  r.push(mk(2));
  r.drop();
  TEST_ASSERT_EQUAL_HEX16(2, r.peek()->id);
  r.drop();
  r.drop();  // vazio: sem efeito
  TEST_ASSERT_TRUE(r.empty());
}

void test_indices_wrap_around_many_times() {
  FrameRing<32> r;
  Frame out;
  for (uint32_t i = 0; i < 2000; i++) {
    TEST_ASSERT_TRUE(r.push(mk((uint16_t)(i & 0x7FF))));
    TEST_ASSERT_TRUE(r.pop(out));
    TEST_ASSERT_EQUAL_HEX16((uint16_t)(i & 0x7FF), out.id);
  }
  TEST_ASSERT_EQUAL_UINT32(0, r.lost());
  TEST_ASSERT_TRUE(r.empty());
}

void test_preserves_frame_content() {
  FrameRing<4> r;
  Frame in = {};
  in.ms = 12345;
  in.id = 0x412;
  in.len = 8;
  for (uint8_t i = 0; i < 8; i++) in.data[i] = (uint8_t)(0xA0 + i);
  r.push(in);
  Frame out;
  TEST_ASSERT_TRUE(r.pop(out));
  TEST_ASSERT_EQUAL_UINT32(12345, out.ms);
  TEST_ASSERT_EQUAL_HEX16(0x412, out.id);
  TEST_ASSERT_EQUAL_UINT8(8, out.len);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(in.data, out.data, 8);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_starts_empty);
  RUN_TEST(test_fifo_order);
  RUN_TEST(test_full_discards_new_frame_and_counts_lost);
  RUN_TEST(test_accepts_again_after_pop);
  RUN_TEST(test_drop_removes_oldest);
  RUN_TEST(test_indices_wrap_around_many_times);
  RUN_TEST(test_preserves_frame_content);
  return UNITY_END();
}

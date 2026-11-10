#include <unity.h>

#include "EngineDynamics.h"

using namespace sniffer;

void setUp() {}
void tearDown() {}

static const uint8_t kRef[8] = {0x1F, 0x58, 0x2D, 0x00, 0x40, 0x2D, 0x2D, 0x2D};

static void expectFrame(const EngineDynamics& d, const uint8_t expected[8]) {
  uint8_t out[8];
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, out, 8);
}

// ---- Requirement: Quadro de referência ----
void test_default_is_the_reference_frame() {
  EngineDynamics d;
  expectFrame(d, kRef);
}

void test_reference_decodes_to_the_captured_values() {
  EngineDynamics d;
  TEST_ASSERT_EQUAL_UINT16(1003, d.rpm());
  TEST_ASSERT_EQUAL_INT16(-10, d.torqueNm());
  TEST_ASSERT_EQUAL_UINT8(0, d.pedal());
  TEST_ASSERT_EQUAL_UINT8(0, d.cruise());
  TEST_ASSERT_FALSE(d.brake());
  TEST_ASSERT_FALSE(d.ac());
  TEST_ASSERT_TRUE(d.diag());
}

void test_cruise_2_on_reference() {
  EngineDynamics d;
  TEST_ASSERT_TRUE(d.setCruise(2));
  const uint8_t exp[8] = {0x1F, 0x58, 0x2D, 0x00, 0x48, 0x2D, 0x2D, 0x2D};
  expectFrame(d, exp);
}

void test_reset_goes_back_to_reference() {
  EngineDynamics d;
  d.setRpm(3000);
  d.setCruise(3);
  d.setBrake(1);
  d.setTorque(110);
  d.reset();
  expectFrame(d, kRef);
}

// ---- Requirement: Campos nomeados e conversões ----
void test_rpm_is_times_8_big_endian() {
  EngineDynamics d;
  TEST_ASSERT_TRUE(d.setRpm(2000));
  uint8_t out[8];
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x3E, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0x80, out[1]);
  d.setRpm(0);
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x00, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0x00, out[1]);
  d.setRpm(8191);
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0xFF, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0xF8, out[1]);
}

void test_torque_110_is_0x69_in_the_four_torque_bytes() {
  EngineDynamics d;
  TEST_ASSERT_TRUE(d.setTorque(110));
  uint8_t out[8];
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x69, out[2]);
  TEST_ASSERT_EQUAL_HEX8(0x69, out[5]);
  TEST_ASSERT_EQUAL_HEX8(0x69, out[6]);
  TEST_ASSERT_EQUAL_HEX8(0x69, out[7]);
}

void test_torque_negative_10_is_0x2D() {
  EngineDynamics d;
  d.setTorque(0);
  TEST_ASSERT_TRUE(d.setTorque(-10));
  uint8_t out[8];
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x2D, out[2]);
}

void test_torque_limits_and_rounding_up() {
  EngineDynamics d;
  uint8_t out[8];
  TEST_ASSERT_TRUE(d.setTorque(-100));
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x00, out[2]);
  TEST_ASSERT_TRUE(d.setTorque(408));
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0xFE, out[2]);
  TEST_ASSERT_TRUE(d.setTorque(-99));  // ímpar: sobe para -98
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x01, out[2]);
  TEST_ASSERT_EQUAL_INT16(-98, d.torqueNm());
}

void test_pedal_is_times_2() {
  EngineDynamics d;
  TEST_ASSERT_TRUE(d.setPedal(50));
  uint8_t out[8];
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x64, out[3]);
  d.setPedal(100);
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0xC8, out[3]);
}

void test_pedal_50_and_cruise_3_give_byte4_0x64_and_byte5_0x4C() {
  EngineDynamics d;
  d.setPedal(50);
  d.setCruise(3);
  uint8_t out[8];
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x64, out[3]);
  TEST_ASSERT_EQUAL_HEX8(0x4C, out[4]);
}

void test_byte5_flags_each_in_its_bit() {
  EngineDynamics d;
  uint8_t out[8];
  d.setDiag(0);
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x00, out[4]);
  d.setBrake(1);
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x02, out[4]);
  d.setBrake(0);
  d.setAc(1);
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x01, out[4]);
  d.setAc(0);
  d.setDiag(1);
  d.setCruise(1);
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x44, out[4]);
  d.setCruise(2);
  d.encode(out);
  TEST_ASSERT_EQUAL_HEX8(0x48, out[4]);
}

// ---- Requirement: Alteração com o envio ligado (campos acumulam) ----
void test_fields_accumulate() {
  EngineDynamics d;
  d.setCruise(2);
  d.setRpm(1500);
  TEST_ASSERT_EQUAL_UINT16(1500, d.rpm());
  TEST_ASSERT_EQUAL_UINT8(2, d.cruise());
}

// ---- Requirement: Rejeição de valores inválidos ----
void test_out_of_range_is_rejected_and_nothing_changes() {
  EngineDynamics d;
  TEST_ASSERT_FALSE(d.setRpm(-1));
  TEST_ASSERT_FALSE(d.setRpm(8192));
  TEST_ASSERT_FALSE(d.setTorque(-101));
  TEST_ASSERT_FALSE(d.setTorque(409));
  TEST_ASSERT_FALSE(d.setPedal(-1));
  TEST_ASSERT_FALSE(d.setPedal(101));
  TEST_ASSERT_FALSE(d.setCruise(4));
  TEST_ASSERT_FALSE(d.setCruise(-1));
  TEST_ASSERT_FALSE(d.setBrake(2));
  TEST_ASSERT_FALSE(d.setAc(-1));
  TEST_ASSERT_FALSE(d.setDiag(5));
  expectFrame(d, kRef);
}

void test_generic_set_dispatches_by_field() {
  EngineDynamics d;
  TEST_ASSERT_TRUE(d.set(TxField::Cruise, 2));
  TEST_ASSERT_TRUE(d.set(TxField::Rpm, 2000));
  TEST_ASSERT_FALSE(d.set(TxField::Cruise, 9));
  TEST_ASSERT_EQUAL_UINT8(2, d.cruise());
  TEST_ASSERT_EQUAL_UINT16(2000, d.rpm());
}

// ---- Requirement: Envio desligado por padrão ----
void test_tx_state_starts_off_with_10ms_period_and_zero_counters() {
  TxState t;
  TEST_ASSERT_FALSE(t.active);
  TEST_ASSERT_EQUAL_UINT16(10, t.periodMs);
  TEST_ASSERT_EQUAL_UINT32(0, t.sent);
  TEST_ASSERT_EQUAL_UINT32(0, t.failed);
  expectFrame(t.dyn, kRef);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_default_is_the_reference_frame);
  RUN_TEST(test_reference_decodes_to_the_captured_values);
  RUN_TEST(test_cruise_2_on_reference);
  RUN_TEST(test_reset_goes_back_to_reference);
  RUN_TEST(test_rpm_is_times_8_big_endian);
  RUN_TEST(test_torque_110_is_0x69_in_the_four_torque_bytes);
  RUN_TEST(test_torque_negative_10_is_0x2D);
  RUN_TEST(test_torque_limits_and_rounding_up);
  RUN_TEST(test_pedal_is_times_2);
  RUN_TEST(test_pedal_50_and_cruise_3_give_byte4_0x64_and_byte5_0x4C);
  RUN_TEST(test_byte5_flags_each_in_its_bit);
  RUN_TEST(test_fields_accumulate);
  RUN_TEST(test_out_of_range_is_rejected_and_nothing_changes);
  RUN_TEST(test_generic_set_dispatches_by_field);
  RUN_TEST(test_tx_state_starts_off_with_10ms_period_and_zero_counters);
  return UNITY_END();
}

#include <unity.h>

#include "IdFilter.h"

using namespace sniffer;

typedef IdFilter<16> Filter;

void setUp() {}
void tearDown() {}

// Requirement: Exibição de todos os IDs por padrão
void test_default_shows_everything() {
  Filter f;
  TEST_ASSERT_FALSE(f.active());
  TEST_ASSERT_TRUE(f.allows(0x000));
  TEST_ASSERT_TRUE(f.allows(0x412));
  TEST_ASSERT_TRUE(f.allows(0x7FF));
}

// Requirement: Definição da lista de IDs exibidos
void test_set_two_ids() {
  Filter f;
  const uint16_t ids[] = {0x412, 0x208};
  TEST_ASSERT_EQUAL(FilterResult::Ok, f.set(ids, 2));
  TEST_ASSERT_TRUE(f.active());
  TEST_ASSERT_TRUE(f.allows(0x412));
  TEST_ASSERT_TRUE(f.allows(0x208));
  TEST_ASSERT_FALSE(f.allows(0x388));
}

void test_set_replaces_previous_list() {
  Filter f;
  const uint16_t a[] = {0x412};
  const uint16_t b[] = {0x388};
  f.set(a, 1);
  TEST_ASSERT_EQUAL(FilterResult::Ok, f.set(b, 1));
  TEST_ASSERT_FALSE(f.allows(0x412));
  TEST_ASSERT_TRUE(f.allows(0x388));
  TEST_ASSERT_EQUAL_UINT8(1, f.count());
}

void test_set_removes_duplicates() {
  Filter f;
  const uint16_t ids[] = {0x412, 0x412, 0x208};
  TEST_ASSERT_EQUAL(FilterResult::Ok, f.set(ids, 3));
  TEST_ASSERT_EQUAL_UINT8(2, f.count());
}

// Requirement: Ajuste incremental da lista
void test_add_to_existing_list() {
  Filter f;
  const uint16_t a[] = {0x412};
  f.set(a, 1);
  TEST_ASSERT_EQUAL(FilterResult::Ok, f.add(0x208));
  TEST_ASSERT_TRUE(f.allows(0x412));
  TEST_ASSERT_TRUE(f.allows(0x208));
  TEST_ASSERT_FALSE(f.allows(0x388));
  TEST_ASSERT_EQUAL_UINT8(2, f.count());
}

void test_add_when_disabled_activates_with_only_that_id() {
  Filter f;
  TEST_ASSERT_EQUAL(FilterResult::Ok, f.add(0x388));
  TEST_ASSERT_TRUE(f.active());
  TEST_ASSERT_TRUE(f.allows(0x388));
  TEST_ASSERT_FALSE(f.allows(0x412));
}

void test_add_is_idempotent() {
  Filter f;
  f.add(0x412);
  f.add(0x412);
  TEST_ASSERT_EQUAL_UINT8(1, f.count());
}

void test_remove_keeps_the_others() {
  Filter f;
  f.add(0x412);
  f.add(0x208);
  f.add(0x388);
  TEST_ASSERT_EQUAL(FilterResult::Ok, f.remove(0x208));
  TEST_ASSERT_EQUAL_UINT8(2, f.count());
  TEST_ASSERT_TRUE(f.allows(0x412));
  TEST_ASSERT_TRUE(f.allows(0x388));
  TEST_ASSERT_FALSE(f.allows(0x208));
}

void test_remove_last_id_disables_filter() {
  Filter f;
  f.add(0x412);
  TEST_ASSERT_EQUAL(FilterResult::Ok, f.remove(0x412));
  TEST_ASSERT_FALSE(f.active());
  TEST_ASSERT_TRUE(f.allows(0x388));
}

void test_remove_absent_id_is_noop() {
  Filter f;
  f.add(0x412);
  TEST_ASSERT_EQUAL(FilterResult::Ok, f.remove(0x208));
  TEST_ASSERT_EQUAL_UINT8(1, f.count());
}

// Requirement: Desativação do filtro
void test_clear_shows_everything_again() {
  Filter f;
  f.add(0x412);
  f.clear();
  TEST_ASSERT_FALSE(f.active());
  TEST_ASSERT_TRUE(f.allows(0x388));
}

// Requirement: Rejeição de comandos inválidos
void test_id_above_0x7FF_is_rejected_and_state_kept() {
  Filter f;
  f.add(0x412);
  TEST_ASSERT_EQUAL(FilterResult::InvalidId, f.add(0x800));
  TEST_ASSERT_EQUAL(FilterResult::InvalidId, f.remove(0x800));
  const uint16_t bad[] = {0x208, 0x800};
  TEST_ASSERT_EQUAL(FilterResult::InvalidId, f.set(bad, 2));
  TEST_ASSERT_EQUAL_UINT8(1, f.count());
  TEST_ASSERT_TRUE(f.allows(0x412));
  TEST_ASSERT_FALSE(f.allows(0x208));
}

void test_max_valid_id_0x7FF_is_accepted() {
  Filter f;
  TEST_ASSERT_EQUAL(FilterResult::Ok, f.add(0x7FF));
  TEST_ASSERT_TRUE(f.allows(0x7FF));
}

void test_capacity_16_ok_and_17_rejected() {
  Filter f;
  for (uint16_t i = 1; i <= 16; i++) TEST_ASSERT_EQUAL(FilterResult::Ok, f.add(i));
  TEST_ASSERT_EQUAL_UINT8(16, f.count());
  TEST_ASSERT_EQUAL(FilterResult::TooMany, f.add(17));
  TEST_ASSERT_EQUAL_UINT8(16, f.count());
  TEST_ASSERT_FALSE(f.allows(17));
}

void test_set_with_more_than_capacity_is_rejected_and_state_kept() {
  Filter f;
  f.add(0x412);
  uint16_t ids[17];
  for (uint8_t i = 0; i < 17; i++) ids[i] = (uint16_t)(0x100 + i);
  TEST_ASSERT_EQUAL(FilterResult::TooMany, f.set(ids, 17));
  TEST_ASSERT_EQUAL_UINT8(1, f.count());
  TEST_ASSERT_TRUE(f.allows(0x412));
}

// Requirement: Filtro não afeta a contagem (o filtro só decide a exibição)
void test_allows_is_pure_decision_and_changes_nothing() {
  Filter f;
  f.add(0x412);
  TEST_ASSERT_FALSE(f.allows(0x388));
  TEST_ASSERT_FALSE(f.allows(0x388));
  TEST_ASSERT_EQUAL_UINT8(1, f.count());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_default_shows_everything);
  RUN_TEST(test_set_two_ids);
  RUN_TEST(test_set_replaces_previous_list);
  RUN_TEST(test_set_removes_duplicates);
  RUN_TEST(test_add_to_existing_list);
  RUN_TEST(test_add_when_disabled_activates_with_only_that_id);
  RUN_TEST(test_add_is_idempotent);
  RUN_TEST(test_remove_keeps_the_others);
  RUN_TEST(test_remove_last_id_disables_filter);
  RUN_TEST(test_remove_absent_id_is_noop);
  RUN_TEST(test_clear_shows_everything_again);
  RUN_TEST(test_id_above_0x7FF_is_rejected_and_state_kept);
  RUN_TEST(test_max_valid_id_0x7FF_is_accepted);
  RUN_TEST(test_capacity_16_ok_and_17_rejected);
  RUN_TEST(test_set_with_more_than_capacity_is_rejected_and_state_kept);
  RUN_TEST(test_allows_is_pure_decision_and_changes_nothing);
  return UNITY_END();
}

#include <string.h>
#include <unity.h>

#include "CommandProcessor.h"

using namespace sniffer;

typedef IdFilter<16> Filter;
typedef CommandProcessor<16> Processor;

// --- captura das respostas ---
static char g_lines[24][100];
static int g_n;

static void capture(const char* text) {
  if (g_n < 24) {
    strncpy(g_lines[g_n], text, 99);
    g_lines[g_n][99] = '\0';
  }
  g_n++;
}

static Filter g_filter;
static Stats g_stats;

void setUp() {
  g_n = 0;
  g_filter = Filter();
  g_stats = Stats();
}
void tearDown() {}

static void feed(Processor& p, const char* s) {
  for (; *s; ++s) p.feed(*s);
}

#define NEW_PROC Processor proc(g_filter, g_stats, capture)

// ---------- f <ids> ----------
void test_set_two_ids() {
  NEW_PROC;
  feed(proc, "f 412 208\n");
  TEST_ASSERT_TRUE(g_filter.active());
  TEST_ASSERT_TRUE(g_filter.allows(0x412));
  TEST_ASSERT_TRUE(g_filter.allows(0x208));
  TEST_ASSERT_FALSE(g_filter.allows(0x388));
  TEST_ASSERT_EQUAL(1, g_n);
  TEST_ASSERT_EQUAL_STRING("# filtro: 412 208", g_lines[0]);
}

void test_set_replaces_previous_list() {
  NEW_PROC;
  feed(proc, "f 412\nf 388\n");
  TEST_ASSERT_FALSE(g_filter.allows(0x412));
  TEST_ASSERT_TRUE(g_filter.allows(0x388));
  TEST_ASSERT_EQUAL_STRING("# filtro: 388", g_lines[1]);
}

void test_accepts_0x_prefix_and_any_case() {
  NEW_PROC;
  feed(proc, "f 0x7e2 0X0aB\n");
  TEST_ASSERT_TRUE(g_filter.allows(0x7E2));
  TEST_ASSERT_TRUE(g_filter.allows(0x0AB));
  TEST_ASSERT_EQUAL_STRING("# filtro: 7E2 0AB", g_lines[0]);
}

void test_command_letter_is_case_insensitive() {
  NEW_PROC;
  feed(proc, "F 412\n");
  TEST_ASSERT_TRUE(g_filter.active());
  feed(proc, "S\n");
  TEST_ASSERT_EQUAL_STRING("# filtro: 412", g_lines[1]);
}

void test_extra_spaces_and_tabs() {
  NEW_PROC;
  feed(proc, "  f \t 412   208  \n");
  TEST_ASSERT_EQUAL_STRING("# filtro: 412 208", g_lines[0]);
}

// ---------- f +id / f -id ----------
void test_add_to_existing_list() {
  NEW_PROC;
  feed(proc, "f 412\nf +208\n");
  TEST_ASSERT_TRUE(g_filter.allows(0x412));
  TEST_ASSERT_TRUE(g_filter.allows(0x208));
  TEST_ASSERT_FALSE(g_filter.allows(0x388));
  TEST_ASSERT_EQUAL_STRING("# filtro: 412 208", g_lines[1]);
}

void test_add_with_filter_off_activates_with_only_that_id() {
  NEW_PROC;
  feed(proc, "f +388\n");
  TEST_ASSERT_TRUE(g_filter.active());
  TEST_ASSERT_FALSE(g_filter.allows(0x412));
  TEST_ASSERT_EQUAL_STRING("# filtro: 388", g_lines[0]);
}

void test_remove_keeps_the_others() {
  NEW_PROC;
  feed(proc, "f 412 208 388\nf -208\n");
  TEST_ASSERT_EQUAL_STRING("# filtro: 412 388", g_lines[1]);
}

void test_remove_last_id_disables_filter() {
  NEW_PROC;
  feed(proc, "f 412\nf -412\n");
  TEST_ASSERT_FALSE(g_filter.active());
  TEST_ASSERT_TRUE(g_filter.allows(0x388));
  TEST_ASSERT_EQUAL_STRING("# filtro: desativado", g_lines[1]);
}

void test_several_signed_ids_in_one_command() {
  NEW_PROC;
  feed(proc, "f 412 208\nf +388 -412\n");
  TEST_ASSERT_EQUAL_STRING("# filtro: 208 388", g_lines[1]);
}

// ---------- f off ----------
void test_off_shows_everything_again() {
  NEW_PROC;
  feed(proc, "f 412\nf off\n");
  TEST_ASSERT_FALSE(g_filter.active());
  TEST_ASSERT_TRUE(g_filter.allows(0x388));
  TEST_ASSERT_EQUAL_STRING("# filtro: desativado", g_lines[1]);
  feed(proc, "f OFF\n");
  TEST_ASSERT_FALSE(g_filter.active());
}

// ---------- erros: o filtro não muda ----------
void test_id_out_of_range_is_rejected_and_filter_kept() {
  NEW_PROC;
  feed(proc, "f 412\nf 800\n");
  TEST_ASSERT_EQUAL(2, g_n);
  TEST_ASSERT_EQUAL_STRING("# erro: ID invalido (hexadecimal, no maximo 7FF)", g_lines[1]);
  TEST_ASSERT_EQUAL_UINT8(1, g_filter.count());
  TEST_ASSERT_TRUE(g_filter.allows(0x412));
}

void test_non_hex_id_is_rejected() {
  NEW_PROC;
  feed(proc, "f 412\nf zz\nf 41g\nf 0x\n");
  TEST_ASSERT_EQUAL(4, g_n);
  for (int i = 1; i < 4; i++) TEST_ASSERT_EQUAL_STRING_LEN("# erro: ID invalido", g_lines[i], 19);
  TEST_ASSERT_EQUAL_UINT8(1, g_filter.count());
}

void test_very_long_hex_does_not_overflow() {
  NEW_PROC;
  feed(proc, "f FFFFFFFFFFFF\n");
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: ID invalido", g_lines[0], 19);
  TEST_ASSERT_FALSE(g_filter.active());
}

void test_f_without_arguments_is_rejected() {
  NEW_PROC;
  feed(proc, "f 412\nf\n");
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: uso:", g_lines[1], 12);
  TEST_ASSERT_EQUAL_UINT8(1, g_filter.count());
}

void test_more_than_16_ids_is_rejected_and_filter_kept() {
  NEW_PROC;
  feed(proc, "f 412\n");
  feed(proc, "f 1 2 3 4 5 6 7 8 9 A B C D E F 10 11\n");  // 17 IDs
  TEST_ASSERT_EQUAL_STRING("# erro: capacidade excedida (maximo 16 IDs)", g_lines[1]);
  TEST_ASSERT_EQUAL_UINT8(1, g_filter.count());
  TEST_ASSERT_TRUE(g_filter.allows(0x412));
}

void test_exactly_16_ids_is_accepted() {
  NEW_PROC;
  feed(proc, "f 1 2 3 4 5 6 7 8 9 A B C D E F 10\n");  // 16 IDs
  TEST_ASSERT_EQUAL_UINT8(16, g_filter.count());
  TEST_ASSERT_EQUAL_STRING_LEN("# filtro: 001 002", g_lines[0], 17);
}

void test_adding_beyond_capacity_with_plus_is_rejected() {
  NEW_PROC;
  feed(proc, "f 1 2 3 4 5 6 7 8 9 A B C D E F 10\nf +300\n");
  TEST_ASSERT_EQUAL_STRING("# erro: capacidade excedida (maximo 16 IDs)", g_lines[1]);
  TEST_ASSERT_EQUAL_UINT8(16, g_filter.count());
  TEST_ASSERT_FALSE(g_filter.allows(0x300));
}

void test_mixing_signed_and_plain_is_rejected() {
  NEW_PROC;
  feed(proc, "f 412\nf 208 +388\nf +388 208\n");
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: nao misture", g_lines[1], 19);
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: nao misture", g_lines[2], 19);
  TEST_ASSERT_EQUAL_UINT8(1, g_filter.count());
  TEST_ASSERT_TRUE(g_filter.allows(0x412));
}

void test_command_is_atomic_when_a_later_id_is_invalid() {
  NEW_PROC;
  feed(proc, "f 412\nf +208 +800\nf 388 800\n");
  TEST_ASSERT_EQUAL_UINT8(1, g_filter.count());
  TEST_ASSERT_TRUE(g_filter.allows(0x412));
  TEST_ASSERT_FALSE(g_filter.allows(0x208));
  TEST_ASSERT_FALSE(g_filter.allows(0x388));
}

void test_unknown_command() {
  NEW_PROC;
  feed(proc, "x\nfoo 1\n");
  TEST_ASSERT_EQUAL(2, g_n);
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: comando desconhecido", g_lines[0], 28);
  TEST_ASSERT_EQUAL_STRING_LEN("# erro: comando desconhecido", g_lines[1], 28);
}

// ---------- s ----------
void test_stats_with_active_filter() {
  NEW_PROC;
  g_stats.received = 1000;
  g_stats.shown = 42;
  g_stats.lost = 3;
  feed(proc, "f 412 208\ns\n");
  TEST_ASSERT_EQUAL(5, g_n);
  TEST_ASSERT_EQUAL_STRING("# filtro: 412 208", g_lines[1]);
  TEST_ASSERT_EQUAL_STRING("# recebidos = 1000", g_lines[2]);
  TEST_ASSERT_EQUAL_STRING("# exibidos = 42", g_lines[3]);
  TEST_ASSERT_EQUAL_STRING("# perdidos = 3", g_lines[4]);
}

void test_stats_without_filter() {
  NEW_PROC;
  feed(proc, "s\n");
  TEST_ASSERT_EQUAL(4, g_n);
  TEST_ASSERT_EQUAL_STRING("# filtro: desativado", g_lines[0]);
  TEST_ASSERT_EQUAL_STRING("# perdidos = 0", g_lines[3]);
}

void test_stats_reads_live_counters() {
  NEW_PROC;
  feed(proc, "s\n");
  g_stats.received = 4294967295UL;
  feed(proc, "s\n");
  TEST_ASSERT_EQUAL_STRING("# recebidos = 4294967295", g_lines[5]);
}

// ---------- tratamento de linha ----------
void test_crlf_produces_a_single_response() {
  NEW_PROC;
  feed(proc, "f 412\r\n");
  TEST_ASSERT_EQUAL(1, g_n);
}

void test_cr_only_and_lf_only_both_end_a_line() {
  NEW_PROC;
  feed(proc, "f 412\rf 208\n");
  TEST_ASSERT_EQUAL(2, g_n);
  TEST_ASSERT_EQUAL_STRING("# filtro: 208", g_lines[1]);
}

void test_empty_and_blank_lines_are_ignored() {
  NEW_PROC;
  feed(proc, "\n\r\n   \n\t\n");
  TEST_ASSERT_EQUAL(0, g_n);
}

void test_command_is_only_executed_at_end_of_line() {
  NEW_PROC;
  feed(proc, "f 412");
  TEST_ASSERT_EQUAL(0, g_n);
  TEST_ASSERT_FALSE(g_filter.active());
  feed(proc, "\n");
  TEST_ASSERT_TRUE(g_filter.active());
}

void test_line_of_exactly_48_chars_is_accepted() {
  NEW_PROC;
  char line[64];
  memset(line, ' ', sizeof(line));
  memcpy(line, "f 412", 5);
  line[48] = '\n';
  line[49] = '\0';
  feed(proc, line);
  TEST_ASSERT_TRUE(g_filter.allows(0x412));
  TEST_ASSERT_EQUAL_STRING("# filtro: 412", g_lines[0]);
}

void test_line_longer_than_48_chars_is_rejected_and_recovers() {
  NEW_PROC;
  char line[80];
  memset(line, ' ', sizeof(line));
  memcpy(line, "f 412", 5);
  line[49] = 'x';  // 50 caracteres
  line[50] = '\n';
  line[51] = '\0';
  feed(proc, line);
  TEST_ASSERT_EQUAL(1, g_n);
  TEST_ASSERT_EQUAL_STRING("# erro: linha muito longa", g_lines[0]);
  TEST_ASSERT_FALSE(g_filter.active());
  feed(proc, "f 208\n");  // volta a funcionar
  TEST_ASSERT_TRUE(g_filter.allows(0x208));
}

void test_every_response_starts_with_hash() {
  NEW_PROC;
  feed(proc, "f 412 208\ns\nf +388\nf -388\nf off\nf 800\nf\nzzz\nf 412 +1\n");
  TEST_ASSERT_TRUE(g_n > 8);
  for (int i = 0; i < g_n && i < 24; i++) TEST_ASSERT_EQUAL_CHAR('#', g_lines[i][0]);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_set_two_ids);
  RUN_TEST(test_set_replaces_previous_list);
  RUN_TEST(test_accepts_0x_prefix_and_any_case);
  RUN_TEST(test_command_letter_is_case_insensitive);
  RUN_TEST(test_extra_spaces_and_tabs);
  RUN_TEST(test_add_to_existing_list);
  RUN_TEST(test_add_with_filter_off_activates_with_only_that_id);
  RUN_TEST(test_remove_keeps_the_others);
  RUN_TEST(test_remove_last_id_disables_filter);
  RUN_TEST(test_several_signed_ids_in_one_command);
  RUN_TEST(test_off_shows_everything_again);
  RUN_TEST(test_id_out_of_range_is_rejected_and_filter_kept);
  RUN_TEST(test_non_hex_id_is_rejected);
  RUN_TEST(test_very_long_hex_does_not_overflow);
  RUN_TEST(test_f_without_arguments_is_rejected);
  RUN_TEST(test_more_than_16_ids_is_rejected_and_filter_kept);
  RUN_TEST(test_exactly_16_ids_is_accepted);
  RUN_TEST(test_adding_beyond_capacity_with_plus_is_rejected);
  RUN_TEST(test_mixing_signed_and_plain_is_rejected);
  RUN_TEST(test_command_is_atomic_when_a_later_id_is_invalid);
  RUN_TEST(test_unknown_command);
  RUN_TEST(test_stats_with_active_filter);
  RUN_TEST(test_stats_without_filter);
  RUN_TEST(test_stats_reads_live_counters);
  RUN_TEST(test_crlf_produces_a_single_response);
  RUN_TEST(test_cr_only_and_lf_only_both_end_a_line);
  RUN_TEST(test_empty_and_blank_lines_are_ignored);
  RUN_TEST(test_command_is_only_executed_at_end_of_line);
  RUN_TEST(test_line_of_exactly_48_chars_is_accepted);
  RUN_TEST(test_line_longer_than_48_chars_is_rejected_and_recovers);
  RUN_TEST(test_every_response_starts_with_hash);
  return UNITY_END();
}

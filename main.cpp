#include "test.hpp"

using namespace testing;

constexpr void add() { assert_eq(1 + 1, 2); }

struct Fixture {
  constexpr void add() const { assert_eq(1 + num, 2); }

  int num = 1;
};

int main() {
  TestSuite suite;

  int total = 0;

  total += TEST_ALL(add).fail_count;
  total += TEST_ALL_FIXTURE(Fixture, &Fixture::add).fail_count;

  total += TEST_ALL_SUITE_FIXTURE(suite, Fixture, &Fixture::add).fail_count;
  total += TEST_ALL_SUITE(suite, add).fail_count;

  total += TEST_ALL_CONSTEXPR(add).fail_count;
  total += TEST_ALL_FIXTURE_CONSTEXPR(Fixture, &Fixture::add).fail_count;

  detail::verify(total == 0);

  return total;
}
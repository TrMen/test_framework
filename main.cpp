#include <thread>

#include "asserts.hpp"
#include "test.hpp"

using namespace testing;

constexpr void add() { assert_eq(1 + 1, 2); }

constexpr void complex() { assert_eq(1, 1); }

void verify_exceptions() {
  constexpr auto nonthrowing = [](int i) { return i + 1; };
  // This assert also return the result of the invocation
  int result = assert_nothrow(FnWithSource{nonthrowing}, 1);
  assert_eq(result, 2);

  auto throwing = [](int i) { throw std::invalid_argument{std::to_string(i)}; };
  // Succeeds on any thrown exception. These return nothing since they are
  // intended to throw.
  assert_throw(FnWithSource{throwing}, 1);
  // This succeeds only if that specific exception is thrown and fails others
  assert_throw<std::invalid_argument>(FnWithSource{throwing}, 1);
  // Expecting a base-class is also fine
  assert_throw<std::exception>(FnWithSource{throwing}, 1);
}

void takes_a_sec() { std::this_thread::sleep_for(std::chrono::seconds(1)); }

struct Fixture {
  constexpr void add() const { assert_eq(1 + num, 2); }

  int num = 1;
};

constexpr int increment(int a) { return a + 1; }

constexpr const char *what_is_it() { return "good"; }

constexpr void using_verify() {
  // To check if the value matches the expected, fail if not, and then return
  // it, you can use v(actual, expected).
  auto two = v(increment(1), 2);
  // For verifying inequality
  const auto *good_ptr = vn(&two, nullptr);

  // Or construct a Verify<T> using the literal _v operator, and then use && to
  // do the same. This will return the value of increment(2).
  auto three = increment(2) && 3_v;
  auto four = 4_v && increment(three);
  auto three_point_five = 2.5_v && 1 + 1.5;
  // Does string comparison
  const auto *str_literal = "good"_v && what_is_it();
  // For inequality checking, use operator^
  const auto *good_ptr_two = &three ^ Verify { nullptr };
  auto twelve = 12 ^ 11_v;
}

int main() {
  TestSuite suite;

  int total = 0;

  total += TEST_ALL(add, takes_a_sec, using_verify).fail_count;
  total += TEST_ALL_FIXTURE(Fixture, &Fixture::add).fail_count;

  total += TEST_ALL_SUITE_FIXTURE(suite, Fixture, &Fixture::add).fail_count;
  total += TEST_ALL_SUITE(suite, add).fail_count;

  total += TEST_ALL_CONSTEXPR(add, complex, using_verify).fail_count;
  total += TEST_ALL_FIXTURE_CONSTEXPR(Fixture, &Fixture::add).fail_count;

  detail::verify(total == 0, "Total is not 0");

  return total;
}
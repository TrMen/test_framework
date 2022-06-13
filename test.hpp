#pragma once

#include <chrono>
#include <experimental/source_location>
#include <fmt/core.h>
#include <stdexcept>
#include <string>

#include "asserts.hpp"
#include "concepts.hpp"
#include "verify.hpp"

namespace testing {

namespace detail {

template <size_t size> struct StringLiteral {
  explicit StringLiteral(const char (&str)[size]) // NOLINT
  {
    std::copy_n(str, size, buff.data());
  }

  std::array<char, size> buff;
};

template <printable... Args>
constexpr void print(const std::string_view fmt_str, Args &&...args) { // NOLINT
  if (not std::is_constant_evaluated()) {
    // TODO: comptime checking doesn't work here, because I'm not in
    // a constexpr context. But fmt_str is logically still comptime,
    // so I need to take only literals somehow
    fmt::vprint(fmt_str, fmt::make_args_checked<Args...>(fmt_str, args...));
  }

  // TODO: Do something sensible for test reporting at compile-time
}

constexpr void verify(bool condition, std::string_view message = "",
                      const std::experimental::source_location loc =
                          std::experimental::source_location::current()) {
  if (not condition) {
    throw std::invalid_argument(fmt::format(
        "{}:{}:{} in {}(): Verfiy failed. Message: '{}'\n", loc.file_name(),
        loc.line(), loc.column(), loc.function_name(), message));
  }
}

constexpr size_t strlen(const char *str) {
  verify(str != nullptr);

  size_t len = 0;
  for (; *str != '\0'; ++str, ++len) {
  }
  return len;
}

constexpr const char *strchr(const char *str, int i) {
  char c = static_cast<char>(i); // This dance is for compliance

  if (str == nullptr) {
    return nullptr;
  }

  for (; *str != '\0' && *str != c; ++str) {
  }

  return (*str == c) ? str : nullptr;
}

struct ConstexprTestSuite {
  constexpr ConstexprTestSuite() = default;

  constexpr void add_failed_test(std::string_view sv) const {
    // TODO: Do something sensible
  }

  [[nodiscard]] constexpr int status() const { return 0; } // NOLINT

  constexpr void increment_total() const { // TODO: Something sensible
  }
  constexpr void increment_failed() const { // TODO: Something sensible
  }

  void report() const { // TODO Something sensible
  }
};

constexpr bool isspace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

constexpr const char *skip_whitespace_and_ampersand(const char *str) {
  if (str == nullptr) {
    return nullptr;
  }

  while (*str != '\0' && isspace(*str) || (*str == '&')) {
    ++str;
  }

  return str;
}

constexpr const char *FAILED = "\033[0;31mFAILED\33[0m";
constexpr const char *PASSED = "\033[0;32mPASSED\33[0m";

} // namespace detail

struct TestSuite {
  TestSuite() : start(std::chrono::system_clock::now()){};

  void add_failed_test(std::string_view sv) { failed_testnames.push_back(sv); }

  [[nodiscard]] int status() const { return failed; }

  void report() const {
    auto end = std::chrono::system_clock::now();
    auto elapsed =
        std::chrono::duration_cast<std::chrono::seconds>(end - start);

    detail::print("\n");
    for (const auto &failed_testname : failed_testnames) {
      detail::print("{}: {}\n", detail::FAILED, failed_testname);
    }
    detail::print("SUMMARY: Ran {} tests in {} seconds. {} failed.\n\n", total,
                  elapsed.count(), failed);
  }

  void increment_total() { total += 1; }
  void increment_failed() { failed += 1; }

  int total = 0;
  int failed = 0;
  std::chrono::time_point<std::chrono::system_clock> start;
  std::vector<std::string_view> failed_testnames{};
};

template <detail::testsuite Suite> struct TestInfo {
  explicit TestInfo(Suite _suite, int fail_c)
      : suite(std::move(_suite)), fail_count(fail_c) {}

  Suite suite;
  int fail_count{};
};

template <detail::testsuite Suite, detail::testcase Fn>
constexpr bool test_single(Suite &test_suite, std::string_view fn_name,
                           Fn &&fn) {
  detail::print("Running {}...\n", fn_name);

  bool passed = false;
  try {
    std::invoke(std::forward<Fn>(fn));
    passed = true;
  } catch (const detail::AssertFailure &e) {
    passed = false;
    fmt::print("{}", e.what());
  }

  detail::print("{}: {}\n\n", passed ? detail::PASSED : detail::FAILED,
                fn_name);

  test_suite.increment_total();

  if (not passed) {
    test_suite.increment_failed();
    test_suite.add_failed_test(fn_name);
  }

  return passed;
}

template <detail::testsuite Suite, detail::testcase Fn, detail::testcase... Fns>
constexpr int test_all(Suite &test_suite, const char *fn_names, Fn &&fn,
                       Fns &&...rest) {
  fn_names = detail::skip_whitespace_and_ampersand(fn_names);
  if constexpr (sizeof...(rest) > 0) {
    const char *pcomma = detail::strchr(fn_names, ',');
    std::string_view fn_name{fn_names, static_cast<size_t>(pcomma - fn_names)};

    bool single_failed =
        !test_single(test_suite, fn_name, std::forward<Fn>(fn));
    auto rest_fails =
        test_all(test_suite, pcomma + 1, std::forward<Fns>(rest)...);

    return static_cast<int>(single_failed) + rest_fails;
  } else {
    std::string_view fn_name{fn_names, detail::strlen(fn_names)};

    return static_cast<int>(
        !test_single(test_suite, fn_name, std::forward<Fn>(fn)));
  }
}

template <detail::testfixture Class, detail::testsuite Suite, typename Method>
requires detail::fixture_testcase<Class, Method> constexpr bool
test_single_with_fixture(Suite &test_suite, std::string_view fn_name,
                         Method &&fn) {
  detail::print("Running {}...\n", fn_name);

  bool passed = false;

  {
    Class fixture;

    try {
      std::invoke(std::forward<Method>(fn), &fixture);
      passed = true;
    } catch (const detail::AssertFailure &e) {
      passed = false;
      fmt::print("{}", e.what());
    }
  }

  detail::print("{}: {}\n\n", passed ? detail::PASSED : detail::FAILED,
                fn_name);

  test_suite.increment_total();

  if (not passed) {
    test_suite.increment_failed();
    test_suite.add_failed_test(fn_name);
  }

  return passed;
}

template <detail::testfixture Klass, detail::testsuite Suite, typename Method,
          typename... Methods>
    requires detail::fixture_testcase<Klass, Method> &&
    (detail::fixture_testcase<Klass, Methods> &&
     ...) constexpr int test_all_with_fixture(Suite &test_suite,
                                              const char *fn_names, Method &&fn,
                                              Methods &&...rest) {
  fn_names = detail::skip_whitespace_and_ampersand(fn_names);
  if constexpr (sizeof...(rest) > 0) {
    const char *pcomma = detail::strchr(fn_names, ',');
    std::string_view fn_name{fn_names, static_cast<size_t>(pcomma - fn_names)};

    bool single_failed = !test_single_with_fixture<Klass>(
        test_suite, fn_name, std::forward<Method>(fn));

    auto rest_fails = test_all_with_fixture<Klass>(
        test_suite, pcomma + 1, std::forward<Methods>(rest)...);

    return rest_fails + static_cast<int>(single_failed);
  } else {
    std::string_view fn_name{fn_names, detail::strlen(fn_names)};

    return static_cast<int>(!test_single_with_fixture<Klass>(
        test_suite, fn_name, std::forward<Method>(fn)));
  }
}

#define TEST_ALL(...)                                                          \
  []() {                                                                       \
    TestSuite suite;                                                           \
    auto fail_c = testing::test_all(suite, #__VA_ARGS__, __VA_ARGS__);         \
    suite.report();                                                            \
    return TestInfo{std::move(suite), fail_c};                                 \
  }()

#define TEST_ALL_FIXTURE(klass, ...)                                           \
  []() {                                                                       \
    TestSuite suite;                                                           \
    auto fail_c = testing::test_all_with_fixture<klass>(suite, #__VA_ARGS__,   \
                                                        __VA_ARGS__);          \
    suite.report();                                                            \
    return TestInfo{std::move(suite), fail_c};                                 \
  }()

#define TEST_ALL_SUITE(suite, ...)                                             \
  [&suite]() {                                                                 \
    auto fail_c = testing::test_all(suite, #__VA_ARGS__, __VA_ARGS__);         \
    suite.report();                                                            \
    return TestInfo{suite, fail_c};                                            \
  }()

#define TEST_ALL_SUITE_FIXTURE(suite, klass, ...)                              \
  [&suite]() {                                                                 \
    auto fail_c = testing::test_all_with_fixture<klass>(suite, #__VA_ARGS__,   \
                                                        __VA_ARGS__);          \
    suite.report();                                                            \
    return TestInfo{suite, fail_c};                                            \
  }()

#define TEST_ALL_CONSTEXPR(...)                                                \
  []() {                                                                       \
    constexpr testing::detail::ConstexprTestSuite suite;                       \
    constexpr auto fail_c =                                                    \
        testing::test_all(suite, #__VA_ARGS__, __VA_ARGS__);                   \
    suite.report();                                                            \
    return TestInfo{suite, fail_c};                                            \
  }()

#define TEST_ALL_FIXTURE_CONSTEXPR(klass, ...)                                 \
  []() {                                                                       \
    constexpr testing::detail::ConstexprTestSuite suite;                       \
    constexpr auto fail_c = testing::test_all_with_fixture<klass>(             \
        suite, #__VA_ARGS__, __VA_ARGS__);                                     \
    return TestInfo{suite, fail_c};                                            \
  }()

} // namespace testing
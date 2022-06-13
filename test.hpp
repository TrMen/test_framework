#pragma once

#include <experimental/source_location>
#include <fmt/core.h>
#include <stdexcept>
#include <string>

#include "asserts.hpp"
#include "concepts.hpp"

namespace testing {

namespace detail {

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

template <printable... Args>
constexpr void print(fmt::format_string<Args...> fmt_str, Args &&...args) {
  if (not std::is_constant_evaluated()) {
    fmt::print(fmt_str, args...);
  }

  // TODO: Do something sensible for test reporting at compile-time
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
  TestSuite() = default;

  TestSuite(const TestSuite &) = delete;
  TestSuite(TestSuite &&) = delete;
  TestSuite &operator=(const TestSuite &) = delete;
  TestSuite &operator=(TestSuite &&) = delete;

  void add_failed_test(std::string_view sv) { failed_testnames.push_back(sv); }

  [[nodiscard]] int status() const { return failed; }

  ~TestSuite() {
    detail::print("\n");
    for (const auto &failed_testname : failed_testnames) {
      detail::print("{}: {}\n", detail::FAILED, failed_testname);
    }
    detail::print("SUMMARY: Ran {} tests. {} failed.\n\n", total, failed);
  }

  void increment_total() { total += 1; }
  void increment_failed() { failed += 1; }

  int total = 0;
  int failed = 0;
  std::vector<std::string_view> failed_testnames;
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

    bool single_passed = test_single(test_suite, fn_name, std::forward<Fn>(fn));
    auto rest_passed =
        test_all(test_suite, pcomma + 1, std::forward<Fns>(rest)...);

    return static_cast<int>(single_passed) + rest_passed;
  } else {
    std::string_view fn_name{fn_names, detail::strlen(fn_names)};

    return static_cast<int>(
        test_single(test_suite, fn_name, std::forward<Fn>(fn)));
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

    bool single_passed = test_single_with_fixture<Klass>(
        test_suite, fn_name, std::forward<Method>(fn));

    auto rest_fails = test_all_with_fixture<Klass>(
        test_suite, pcomma + 1, std::forward<Methods>(rest)...);

    return rest_fails + static_cast<int>(single_passed);
  } else {
    std::string_view fn_name{fn_names, detail::strlen(fn_names)};

    return static_cast<int>(test_single_with_fixture<Klass>(
        test_suite, fn_name, std::forward<Method>(fn)));
  }
}

#define TEST_ALL(...)                                                          \
  []() {                                                                       \
    TestSuite suite;                                                           \
    return testing::test_all(suite, #__VA_ARGS__, __VA_ARGS__);                \
  }()

#define TEST_ALL_FIXTURE(klass, ...)                                           \
  []() {                                                                       \
    TestSuite suite;                                                           \
    return testing::test_all_with_fixture<klass>(suite, #__VA_ARGS__,          \
                                                 __VA_ARGS__);                 \
  }()

#define TEST_ALL_SUITE(suite, ...)                                             \
  testing::test_all(suite, #__VA_ARGS__, __VA_ARGS__)

#define TEST_ALL_SUITE_FIXTURE(suite, klass, ...)                              \
  testing::test_all_with_fixture<klass>(suite, #__VA_ARGS__, __VA_ARGS__)

#define TEST_ALL_CONSTEXPR(...)                                                \
  []() {                                                                       \
    constexpr detail::ConstexprTestSuite suite;                                \
    /* TODO: Is there a nice way to check                                      \
    if all passed function are constexpr? */                                   \
    constexpr bool passed =                                                    \
        testing::test_all(suite, #__VA_ARGS__, __VA_ARGS__);                   \
    return passed;                                                             \
  }()

#define TEST_ALL_FIXTURE_CONSTEXPR(klass, ...)                                 \
  []() {                                                                       \
    constexpr detail::ConstexprTestSuite suite;                                \
    constexpr bool passed = testing::test_all_with_fixture<klass>(             \
        suite, #__VA_ARGS__, __VA_ARGS__);                                     \
    return passed;                                                             \
  }()

} // namespace testing
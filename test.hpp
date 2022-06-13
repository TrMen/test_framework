#pragma once

#include <concepts>
#include <experimental/source_location>
#include <fmt/core.h>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>

namespace testing {

struct TestFailure : std::logic_error {
  explicit TestFailure(const std::string &what) : std::logic_error(what) {}
};

namespace detail {
template <typename T> concept printable = requires(T &&t) {
  { std::cout << std::forward<T>(t) }
  ->std::convertible_to<std::ostream &>;
};

template <typename T> concept testcase = requires(T &&t) {
  { std::invoke(std::forward<T>(t)) }
  ->std::same_as<void>;
};

template <typename T> concept testfixture = requires(T &&t) {
  requires std::is_default_constructible_v<T>;
};

template <typename Class, typename Method>
concept fixture_testcase = requires(Class fixture, Method &&method) {
  requires testfixture<Class>;

  { std::invoke(std::forward<Method>(method), &fixture) }
  ->std::same_as<void>;
};

template <typename T>
concept testsuite = requires(T suite, std::string_view sv) {
  { suite.status() }
  ->std::same_as<int>;

  suite.add_failed_test(sv);

  suite.increment_total();
  suite.increment_failed();
};

template <typename T> struct constexpr_context_helper {};

constexpr void verify(bool condition, std::string_view message = "",
                      const std::experimental::source_location loc =
                          std::experimental::source_location::current()) {
  if (not condition) {
    throw std::invalid_argument(
        fmt::format("{}:{}:{} in {}(): {}\n", loc.file_name(), loc.line(),
                    loc.column(), loc.function_name(), message));
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

template <typename T> std::string to_string(T &&val) {
  if constexpr (printable<T>) {
    std::stringstream s;
    s << std::forward<T>(val);
    return s.str();
  } else if constexpr (std::is_enum_v<T &&>) {
    return std::to_string(
        static_cast<std::underlying_type_t<T>>(std::forward<T>(val)));
  } else {
    return "<UNPRINTABLE>";
  }
}

[[noreturn]] inline void fail(std::experimental::source_location location,
                              std::string_view str) {
  throw TestFailure{fmt::format("{}:{}:{} in {}(): {}\n", location.file_name(),
                                location.line(), location.column(),
                                location.function_name(), str)};
}

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

template <typename... Args> std::string args_string(Args &&...args) {
  std::stringstream str;
  ((str << to_string(std::forward<Args>(args)) << ", "), ...);

  return str.str();
}

} // namespace detail

template <typename Lhs, typename Rhs>
requires std::equality_comparable_with<Lhs, Rhs> constexpr void
assert_eq(Lhs &&lhs, Rhs &&rhs,
          const std::experimental::source_location location =
              std::experimental::source_location::current()) {
  if (lhs != rhs) {
    detail::fail(location,
                 fmt::format("ASSERT: '{}' and '{}' are not equal",
                             detail::to_string(std::forward<Lhs>(lhs)),
                             detail::to_string(std::forward<Rhs>(rhs))));
  }
}

constexpr void assert_true(bool val,
                           const std::experimental::source_location location =
                               std::experimental::source_location::current()) {
  if (not val) {
    detail::fail(location, "ASSERT: Value is false");
  }
}

template <typename Fn, typename... Args>
requires std::invocable<Fn, Args...> constexpr void
assert_nothrow(Fn &&fn, Args &&...args,
               const std::experimental::source_location location =
                   std::experimental::source_location::current()) {
  try {
    // Can't forward args here, because it's potentially used in the catch
    std::invoke(std::forward<Fn>(fn), args...);
  } catch (const std::exception &e) {
    auto args_str = detail::args_string(std::forward<Args>(args)...);
    detail::fail(location,
                 fmt::format("ASSERT: Unexpected std::exception thrown "
                             "with arguments '{}'. what(): '{}'",
                             std::move(args_str), e.what()));

  } catch (...) {
    auto args_str = detail::args_string(std::forward<Args>(args)...);
    detail::fail(
        location,
        fmt::format("ASSERT: Unknown exception thrown with arguments '{}'",
                    std::move(args_str)));
  }
}

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
  } catch (const TestFailure &) {
    passed = false;
  }

  detail::print("{}: {}\n", passed ? detail::PASSED : detail::FAILED, fn_name);

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
    } catch (const TestFailure &) {
      passed = false;
    }
  }

  detail::print("{}: {}\n", passed ? detail::PASSED : detail::FAILED, fn_name);

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

#define TEST_ALL(suite, ...) testing::test_all(suite, #__VA_ARGS__, __VA_ARGS__)

#define TEST_ALL_CONSTEXPR(...)                                                \
  []() {                                                                       \
    constexpr detail::ConstexprTestSuite suite;                                \
    /* TODO: Is there a nice way to check                                      \
    if all passed function are constexpr? */                                   \
    constexpr bool passed =                                                    \
        testing::test_all(suite, #__VA_ARGS__, __VA_ARGS__);                   \
    return passed;                                                             \
  }()

#define TEST_ALL_WITH_FIXTURE(suite, klass, ...)                               \
  testing::test_all_with_fixture<klass>(suite, #__VA_ARGS__, __VA_ARGS__)

#define TEST_ALL_WITH_FIXTURE_CONSTEXPR(klass, ...)                            \
  []() {                                                                       \
    constexpr detail::ConstexprTestSuite suite;                                \
    constexpr bool passed = testing::test_all_with_fixture<klass>(             \
        suite, #__VA_ARGS__, __VA_ARGS__);                                     \
    return passed;                                                             \
  }()

} // namespace testing
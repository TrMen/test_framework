#pragma once

#include <atomic>
#include <concepts>
#include <experimental/source_location>
#include <fmt/core.h>
#include <iostream>
#include <mutex>
#include <optional>
#include <source_location>
#include <sstream>
#include <string>
#include <thread>
#include <type_traits>

namespace testing {

struct TestFailure : std::exception {};

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

inline void print(std::experimental::source_location location,
                  std::string_view str) {
  fmt::print("{}:{}:{} in {}(): {}\n", location.file_name(), location.line(),
             location.column(), location.function_name(), str);
}

template <typename T> std::string str(const T &val) {
  if constexpr (detail::printable<T>) {
    std::stringstream s;
    s << val;
    return s.str();
  } else if constexpr (std::is_enum_v<T>) {
    return std::to_string(static_cast<std::underlying_type_t<T>>(val));
  } else {
    return "<UNPRINTABLE>";
  }
}

[[noreturn]] inline void fail() { throw TestFailure{}; }

constexpr const char *skip_whitespace_and_ampersand(const char *str) {
  if (str == nullptr) {
    return nullptr;
  }

  while (*str != '\0' && (isspace(*str) != 0) || (*str == '&')) {
    ++str;
  }

  return str;
}

constexpr const char *FAILED = "\033[0;31mFAILED\33[0m";
constexpr const char *PASSED = "\033[0;32mPASSED\33[0m";

template <typename... Ts> std::string args_string(Ts &&...args) {
  std::stringstream str;
  ((str << args << ", "), ...);

  return str.str();
}

} // namespace detail

template <typename Lhs, typename Rhs>
requires std::equality_comparable_with<Lhs, Rhs> void
assert_eq(const Lhs &lhs, const Rhs &rhs,
          const std::experimental::source_location location =
              std::experimental::source_location::current()) {
  if (lhs != rhs) {
    detail::print(location, fmt::format("ASSERT: '{}' and '{}' are not equal",
                                        detail::str(lhs), detail::str(rhs)));
    detail::fail();
  }
}

inline void assert_true(bool val,
                        const std::experimental::source_location location =
                            std::experimental::source_location::current()) {
  if (not val) {
    detail::print(location, "ASSERT: Value is false");
    detail::fail();
  }
}

template <typename Fn, typename... Args>
requires std::invocable<Fn, Args...> void
assert_nothrow(Fn &&fn, Args &&...args,
               const std::experimental::source_location location =
                   std::experimental::source_location::current()) {
  try {
    std::invoke(std::forward<Fn>(fn), std::forward<Args>(args)...);
  } catch (const std::exception &e) {
    auto args_str = detail::args_string(detail::str(args)...);
    detail::print(location,
                  fmt::format("ASSERT: Unexpected std::exception thrown "
                              "with arguments '{}'. what(): '{}'",
                              args_str, e.what()));

    detail::fail();
  } catch (...) {
    auto args_str = detail::args_string(detail::str(args)...);
    detail::print(
        location,
        fmt::format("ASSERT: Unknown exception thrown with arguments '{}'",
                    args_str));
    detail::fail();
  }
}

struct TestSuite {
  TestSuite() = default;

  TestSuite(const TestSuite &) = delete;
  TestSuite(TestSuite &&) = delete;
  TestSuite &operator=(const TestSuite &) = delete;
  TestSuite &operator=(TestSuite &&) = delete;

  [[nodiscard]] int status() {
    std::lock_guard lock{mut};
    return failed;
  }

  ~TestSuite() {
    std::lock_guard lock{mut};

    fmt::print("\n");
    for (const auto &failed_testname : failed_testnames) {
      fmt::print("{}: {}\n", detail::FAILED, failed_testname);
    }
    fmt::print("SUMMARY: Ran {} tests. {} failed.\n\n", total.load(), failed);
  }

  std::atomic<int> total = 0;
  int failed = 0;
  std::mutex mut;
  std::vector<std::string_view> failed_testnames;
};

template <detail::testcase Fn>
bool test_single(TestSuite &testInfo, std::string_view fn_name, Fn &&fn) {
  fmt::print("Running {}...\n", fn_name);

  bool passed = false;
  try {
    std::invoke(std::forward<Fn>(fn));
    passed = true;
  } catch (const TestFailure &) {
    passed = false;
  }

  fmt::print("{}: {}\n", passed ? detail::PASSED : detail::FAILED, fn_name);

  testInfo.total += 1;

  if (not passed) {
    std::lock_guard lock{testInfo.mut};
    testInfo.failed += 1;
    testInfo.failed_testnames.push_back(fn_name);
  }

  return passed;
}

template <typename Fn, typename... Fns>
    requires detail::testcase<Fn> &&
    (detail::testcase<Fns> && ...) int test_all(TestSuite &testInfo,
                                                const char *fn_names, Fn &&fn,
                                                Fns &&...rest) {
  fn_names = detail::skip_whitespace_and_ampersand(fn_names);
  if constexpr (sizeof...(rest) > 0) {
    const char *pcomma = strchr(fn_names, ',');
    std::string_view fn_name{fn_names, static_cast<size_t>(pcomma - fn_names)};

    bool single_passed = test_single(testInfo, fn_name, std::forward<Fn>(fn));
    auto rest_passed =
        test_all(testInfo, pcomma + 1, std::forward<Fns>(rest)...);

    return static_cast<int>(single_passed) + rest_passed;
  } else {
    std::string_view fn_name{fn_names, strlen(fn_names)};

    return static_cast<int>(
        test_single(testInfo, fn_name, std::forward<Fn>(fn)));
  }
}

template <typename Class, typename Method>
requires detail::testfixture<Class> &&
    detail::fixture_testcase<Class, Method> bool
    test_single_with_fixture(TestSuite &testInfo, std::string_view fn_name,
                             Method &&fn) {
  fmt::print("Running {}...\n", fn_name);

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

  fmt::print("{}: {}\n", passed ? detail::PASSED : detail::FAILED, fn_name);

  testInfo.total += 1;

  if (not passed) {
    std::lock_guard lock{testInfo.mut};
    testInfo.failed += 1;
    testInfo.failed_testnames.push_back(fn_name);
  }

  return passed;
}

template <typename Klass, typename Method, typename... Methods>
    requires detail::testfixture<Klass> &&
        detail::fixture_testcase<Klass, Method> &&
    (detail::fixture_testcase<Klass, Methods> &&
     ...) int test_all_with_fixture(TestSuite &testInfo, const char *fn_names,
                                    Method &&fn, Methods &&...rest) {
  fn_names = detail::skip_whitespace_and_ampersand(fn_names);
  if constexpr (sizeof...(rest) > 0) {
    const char *pcomma = strchr(fn_names, ',');
    std::string_view fn_name{fn_names, static_cast<size_t>(pcomma - fn_names)};

    bool single_passed = test_single_with_fixture<Klass>(
        testInfo, fn_name, std::forward<Method>(fn));

    auto rest_fails = test_all_with_fixture<Klass>(
        testInfo, pcomma + 1, std::forward<Methods>(rest)...);

    return rest_fails + static_cast<int>(single_passed);
  } else {
    std::string_view fn_name{fn_names, strlen(fn_names)};

    return static_cast<int>(test_single_with_fixture<Klass>(
        testInfo, fn_name, std::forward<Method>(fn)));
  }
}

#define TEST_ALL(suite, ...) testing::test_all(suite, #__VA_ARGS__, __VA_ARGS__)

#define TEST_ALL_WITH_FIXTURE(suite, klass, ...)                               \
                                                                               \
  testing::test_all_with_fixture<klass>(suite, #__VA_ARGS__, __VA_ARGS__)

} // namespace testing
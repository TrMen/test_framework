#pragma once

#include <concepts>
#include <functional>
#include <iostream>

namespace testing::detail {
template <typename T> concept printable = requires(T &&t) {
  { std::cout << std::forward<T>(t) }
  ->std::convertible_to<std::ostream &>;
};

template <printable T> struct s {};

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

  suite.report();
};

} // namespace testing::detail
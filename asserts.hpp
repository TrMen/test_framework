#pragma once

#include <concepts>
#include <experimental/source_location>
#include <fmt/core.h>
#include <sstream>
#include <stdexcept>

#include "concepts.hpp"

namespace testing {

namespace detail {
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

struct AssertFailure : std::logic_error {
  explicit AssertFailure(const std::string &what) : std::logic_error(what) {}
};

[[noreturn]] inline void fail(std::experimental::source_location location,
                              std::string_view str) {
  throw AssertFailure{fmt::format(
      "{}:{}:{} in {}(): {}\n", location.file_name(), location.line(),
      location.column(), location.function_name(), str)};
}

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

// Helper struct to allow variadic template pack and then a defaulted source
// location. This instanciates the source location in it's constructor.
// TODO: I don't like that the user has to explicitly use this.
template <typename T> struct FnWithSource {
  explicit FnWithSource(T _fn,
                        const std::experimental::source_location _location =
                            std::experimental::source_location::current())
      : fn(std::move(_fn)), location(_location) {}

  T fn;
  std::experimental::source_location location;
};

constexpr void assert_true(bool val,
                           const std::experimental::source_location location =
                               std::experimental::source_location::current()) {
  if (not val) {
    detail::fail(location, "ASSERT: Value is false");
  }
}

constexpr void assert_false(bool val,
                            const std::experimental::source_location location =
                                std::experimental::source_location::current()) {
  if (val) {
    detail::fail(location, "ASSERT: Value is true");
  }
}

template <typename Fn, typename... Args>
requires std::invocable<Fn, Args...> constexpr void
assert_nothrow(const FnWithSource<Fn> &fn, Args &&...args) {
  try {
    // Can't forward args here, because it's potentially used in the catch
    std::invoke(fn.fn, args...);

  } catch (const std::exception &e) {
    auto args_str = detail::args_string(std::forward<Args>(args)...);
    detail::fail(fn.location,
                 fmt::format("ASSERT: Unexpected std::exception thrown "
                             "with arguments '{}'. what(): '{}'",
                             std::move(args_str), e.what()));
  } catch (...) {
    auto args_str = detail::args_string(std::forward<Args>(args)...);
    detail::fail(
        fn.location,
        fmt::format(
            "ASSERT: Unexpected unknown exception thrown with arguments '{}'",
            std::move(args_str)));
  }
}

// Helper that can be supplied to assert_throw to match any thrown exception
struct AnyException {};

template <typename Exception = AnyException, typename Fn, typename... Args>
requires std::invocable<Fn, Args...> constexpr void
assert_throw(FnWithSource<Fn> fn, Args &&...args) {
  if constexpr (std::is_same_v<Exception, AnyException>) {
    try {
      // Can't forward args here, because it's used later
      std::invoke(fn.fn, args...);
    } catch (...) {
      return; // PASSED
    }
  } else {
    try {
      // Can't forward args here, because it's used later
      std::invoke(fn.fn, args...);
    } catch (const Exception &) {
      return; // PASSED
    } catch (const std::exception &e) {
      auto args_str = detail::args_string(std::forward<Args>(args)...);
      detail::fail(fn.location,
                   fmt::format("ASSERT:Invokation threw exception of "
                               "unexpected type derived from std::exception "
                               "with arguments '{}'. what(): '{}'",
                               std::move(args_str), e.what()));
    } catch (...) {
      auto args_str = detail::args_string(std::forward<Args>(args)...);
      detail::fail(
          fn.location,
          fmt::format("ASSERT: Invokation threw exception of unexpected and "
                      "unknown type with arguments '{}'",
                      std::move(args_str)));
    }
  }

  auto args_str = detail::args_string(std::forward<Args>(args)...);
  detail::fail(
      fn.location,
      fmt::format(
          "ASSERT: Invokation did not throw an exception with arguments '{}'",
          std::move(args_str)));
}

} // namespace testing
#pragma once

#include "concepts.hpp"
#include <cstddef>

namespace testing {

template <typename T> struct Verify {
  constexpr explicit Verify(T _val) : val(std::move(_val)) {}
  T val;
};

constexpr Verify<unsigned long long>      // NOLINT: uint64_t not allowed
operator"" _v(unsigned long long literal) // NOLINT: uint64_t not allowed
{
  return Verify(literal);
}

constexpr Verify<std::string_view> operator"" _v(const char *literal,
                                                 size_t size) {
  return Verify(std::string_view(literal, size));
}

constexpr Verify<long double> operator"" _v(long double literal) {
  return Verify(literal);
}

constexpr Verify<char> operator"" _v(char literal) { return Verify(literal); }

template <typename T, typename Other>
requires std::equality_comparable_with<T, Other> constexpr Other
operator&&(Other &&actual, const Verify<T> &expected) {
  if (actual != expected.val) {
    throw std::invalid_argument("Lhs != rhs");
  }

  // Since c++20, rvalue refs are automatically moved as well.
  // So this should be maximally effficient. lvalue refs are returned
  // by ref, rvalue refs are moved.
  return actual;
}

template <typename T, typename Other>
requires std::equality_comparable_with<T, Other> constexpr Other
operator&&(const Verify<T> &expected, Other &&actual) {
  if (actual != expected.val) {
    throw std::invalid_argument("Lhs != rhs");
  }

  return actual;
}

template <typename T, typename Other>
requires std::equality_comparable_with<T, Other> constexpr Other
operator^(const Verify<T> &expected, Other &&actual) {
  if (actual == expected.val) {
    throw std::invalid_argument("Lhs != rhs");
  }

  return actual;
}

template <typename T, typename Other>
requires std::equality_comparable_with<T, Other> constexpr Other
operator^(Other &&actual, const Verify<T> &expected) {
  if (actual == expected.val) {
    throw std::invalid_argument("Lhs != rhs");
  }

  return actual;
}

template <typename T, typename Other>
requires std::equality_comparable_with<T, Other> constexpr Other
v(Other &&actual, T &&expected) {
  if (actual != std::forward<T>(expected)) {
    throw std::invalid_argument("Lhs != rhs");
  }

  return actual;
}

template <typename T, typename Other>
requires std::equality_comparable_with<T, Other> constexpr Other
vn(Other &&actual, T &&expected) {
  if (actual == std::forward<T>(expected)) {
    throw std::invalid_argument("Lhs != rhs");
  }

  return actual;
}

} // namespace testing
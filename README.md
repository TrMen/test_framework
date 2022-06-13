A simple experimental C++20 unit test framework. The key feature is that tests may be run at runtime or compiletime.

Tests are just simple functions that use the `testing::assert_*` functions for assertions.
```C++
#include "asserts.hpp"
#include "test.hpp"

using namespace testing;

constexpr void add() { 
  assert_eq(1 + 1, 2); 
}

constexpr void fail1() {
  // Test will fail and report location and assertion arguments
  assert_eq(1, 2);
}

constexpr void fail2() {
  // Test will fail
  assert_nothrow(FnWithSource{ []() { throw std::invalid_argument("bad"); }}); 
}

struct Fixture {
  constexpr void add() const { assert_eq(1 + num, 2); }

  int num = 1;
};

int main() {
  // Runtime tests, with runtime reporting
  TEST_ALL(add, fail1, fail2);
  // Each test with a fixture is run with a new instance of that class
  TEST_ALL_FIXTURE(Fixture, &Fixture::add); 

  TestSuite suite; 
  // Tests run with the same test suite are reported together.
  TEST_ALL_SUITE_FIXTURE(suite, Fixture, &Fixture::add);
  TEST_ALL_SUITE(suite, add);

  // These tests are forced to run at compiletime.
  // If they fail, compilation will fail.
  TEST_ALL_CONSTEXPR(add, fail1, fail2);
  TEST_ALL_FIXTURE_CONSTEXPR(Fixture, &Fixture::add);
}
```

The experimental idea is that a lot of functions can be `constexpr` annotated. Especially the kind of functions you would usually use unit tests for, since they should be self-contained.

Any constexpr-annotated function can be run at compiletime with `TEST_ALL_CONSTEXPR`, and if any contained assertion fails, compilation will fail deliberately. 

The workflow here is that you'd use some IDE with a language server that does macro expansion and constexpr variable evaluation. Then your IDE will immediately underline failed tests as you are writing them!

But you can also run tests as usual at runtime, if you want a report of run and failed tests, or have tests that you can't or don't want to run at compiletime.

The program above (after removing the failing compiletime test), will generate the following output at runtime:
```
Running add...
PASSED: add

Running fail1...
../main.cpp:10:0 in fail1(): ASSERT: '1' and '2' are not equal
FAILED: fail1

Running fail2...
../main.cpp:14:0 in fail2(): ASSERT: Unexpected std::exception thrown with arguments ''. what(): 'bad'
FAILED: fail2


FAILED: fail1
FAILED: fail2
SUMMARY: Ran 3 tests in 0 seconds. 2 failed.

Running Fixture::add...
PASSED: Fixture::add


SUMMARY: Ran 1 tests in 0 seconds. 0 failed.

Running Fixture::add...
PASSED: Fixture::add


SUMMARY: Ran 1 tests in 0 seconds. 0 failed.

Running add...
PASSED: add


SUMMARY: Ran 2 tests in 0 seconds. 0 failed.
```

For some more usage examples, look in `main.cpp`.

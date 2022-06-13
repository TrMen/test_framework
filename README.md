# Ideas for improvement:

# Failed ideas for improvement:
- Use #pragma message for comptime reporting
  - Preprocessor runs before compilation, so any message would trigger regardless of test success.
- Use one macro and auto-detect if the function is marked constexpr
  - No way to differentiate between function not marked constexpr and function that threw an exception at compile-time
  - `consteval` doesn't work either. I thought maybe you could go the other way, and `if constexpr (is_runtime_executable(f))`, 
    which would fail for `consteval` functions. But I don't think there's any way to specify this which doesn't just fail compilation right there.
- FnWithSource implicitly constructed for assert_nothrow
  - The variadic template argument afterwards can't be skipped in any way. 
  - Maybe tuple would work, but I don't wanna do that.
- Report comptime failure informations after the fact to differentiate failed tests from non-constexpr tests.
  - Would require a compile-time mutable counter. I don't think that's (easily) possible. Or some convoluted functional solution I won't implement
  - Would also lose accurate error locations.
# Failed ideas for improvement:
- Use #pragma message for comptime reporting
  - Preprocessor runs before compilation, so any message would trigger regardless of test success.
- Use one macro and auto-detect if the function is marked constexpr
  - No way to differentiate between function not marked constexpr and function that threw an exception at compile-time
- FnWithSource implicitly constructed for assert_nothrow
  - The variadic template argument afterwards can't be skipped in any way. 
  - Maybe tuple would work, but I don't wanna do that.

---
name: pioTest
description: Run PlatformIO native tests and provide a summary of results with detailed failure explanations. Use when user wants to run tests, check test status, or debug test failures in a PlatformIO project.
---

# PlatformIO Native Test Runner

Runs native PlatformIO tests and provides a clear summary of results with detailed failure information.

## Quick Start

1. Find the project root (directory containing `platformio.ini`)
2. Run `pio test -e native`
3. Parse output for pass/fail counts and failure details
4. Present clean summary with failures highlighted

## Workflow

### Step 1: Locate Project Root
Look for `platformio.ini` in current directory or parent directories.

### Step 2: Run Tests
Execute `pio test -e native` and capture full output.

### Step 3: Parse Results
Extract from output:
- Test suite names (e.g., `[test_button]`)
- Total tests/assertions counts
- Pass/fail status per suite
- Failure details:
  - Test case name
  - File path and line number
  - Failed assertion
  - Expected vs actual values

### Step 4: Format Summary

**If all tests pass:**
```
✅ All tests passed!

Summary:
- 7 test suites
- 72 tests
- 132 assertions
- 0 failures
```

**If tests fail:**
```
❌ Some tests failed

Summary:
- 7 test suites
- 70 tests passed, 2 tests failed
- 132 assertions

Failures:

test_button:
  ❌ Button::update should detect press after debounce period
     Location: test/test_button/test_main.cpp:45
     Failed: REQUIRE(button.isPressed() == true)
     Expected: true
     Actual: false

test_turnout:
  ❌ Turnout::setPosition should not move when locked
     Location: test/test_turnout/test_main.cpp:89
     Failed: REQUIRE(turnout.getPosition() == TurnoutPosition::Normal)
     Expected: Normal
     Actual: Reverse
```

## Notes

- Only runs `native` environment (no hardware required)
- Works with Catch2 test framework
- Preserves full error context for debugging
- Exit code: 0 if all tests pass, non-zero if any fail
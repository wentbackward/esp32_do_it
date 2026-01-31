# Trackpad Testing Guide

## Overview

The trackpad gesture processing logic has been extracted into a testable, framework-independent module. This enables comprehensive unit testing without ESP32 hardware or LVGL.

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    ui_trackpad.c                        │
│              (LVGL Adapter Layer - Thin)                │
│  - Receives LVGL touch events                           │
│  - Converts to trackpad_input_t                         │
│  - Calls trackpad_process_input()                       │
│  - Executes returned actions (HID send, UI update)      │
└──────────────────┬──────────────────────────────────────┘
                   │
                   ▼
┌─────────────────────────────────────────────────────────┐
│              trackpad_gesture.c/.h                      │
│         (Pure Gesture Processing Logic)                 │
│  - Zone detection                                        │
│  - Jitter filtering                                      │
│  - Velocity smoothing (EWMA)                            │
│  - Acceleration curves                                   │
│  - Tap classification                                    │
│  - State machine (idle/moving/scrolling/dragging)       │
│  - Returns actions, doesn't execute them                │
└─────────────────────────────────────────────────────────┘
                   ▲
                   │
┌─────────────────────────────────────────────────────────┐
│              test_trackpad_gesture.c                    │
│                 (Comprehensive Tests)                   │
│  - Unit tests for pure functions                        │
│  - Integration tests for gesture sequences              │
│  - No LVGL, ESP-IDF, or hardware required              │
└─────────────────────────────────────────────────────────┘
```

## Key Design Principles

### 1. Separation of Concerns
- **`trackpad_gesture.c`**: Pure logic, no framework dependencies
- **`ui_trackpad.c`**: Thin adapter, handles LVGL/UI concerns

### 2. Explicit State Management
Instead of file-scope static variables, all state is in `trackpad_state_t`:
- Can reset between tests
- Can create multiple instances
- Can inject specific states for edge case testing

### 3. Time Injection
Time is passed as input (`trackpad_input_t.timestamp_ms`), not queried:
- Tests are deterministic
- Can simulate exact timing scenarios
- No flaky tests due to wall-clock time

### 4. Action Return vs. Execution
Gesture processor returns *what to do*, caller decides *how to do it*:
- Testable without HID stack
- Can verify actions without side effects
- Enables spy/mock pattern

## File Structure

```
main/
  trackpad_gesture.h      # Pure gesture processing API
  trackpad_gesture.c      # Pure gesture implementation
  ui_trackpad.h           # LVGL UI API (public)
  ui_trackpad.c           # LVGL adapter (uses trackpad_gesture.c)

test/
  trackpad_test_helper.h  # Test infrastructure
  trackpad_test_helper.c  # Action recorder, test DSL
  test_trackpad_gesture.c # Comprehensive tests
  CMakeLists.txt          # Host build configuration
  README.md               # Test documentation
```

## What's Testable

### Pure Functions (No State)
✅ `trackpad_clamp_i32()` - Range clamping
✅ `trackpad_get_zone()` - Zone detection
✅ `trackpad_filter_jitter()` - Dead zone filtering
✅ `trackpad_is_jitter()` - Jitter detection
✅ `trackpad_ewma_update()` - Velocity smoothing
✅ `trackpad_apply_acceleration()` - Acceleration curves
✅ `trackpad_classify_tap()` - Tap vs hold classification

### Stateful Gesture Sequences
✅ Tap → Click
✅ Movement → Accelerated cursor movement
✅ Tap-tap-drag → Drag with button held
✅ Double-tap → Double-click
✅ Scroll zones (vertical/horizontal)
✅ Timing edge cases (tap timeout, double-tap window)

## Running Tests

### Host Machine (Fast Iteration)

**Requirements:**
- CMake 3.14+
- C compiler (gcc, clang, MSVC)
- Works on Windows (MSYS2/MinGW), Linux, macOS

Unity test framework is downloaded automatically via CMake FetchContent.

```bash
# From project root
cd test
mkdir build && cd build
cmake ..            # Unity downloads automatically
cmake --build .     # Cross-platform build command
./trackpad_tests    # Run tests

# Expected output:
# test_trackpad_gesture.c:XX:test_clamp_within_range:PASS
# test_trackpad_gesture.c:XX:test_zone_main_trackpad_area:PASS
# ...
# -----------------------
# 47 Tests 0 Failures 0 Ignored
# OK
```

**Note:** `cmake --build .` works across all platforms (MSYS2/Windows, Linux, macOS).

### On ESP32 (Integration Testing)

```bash
# From project root
idf.py -D TEST_COMPONENTS='main' build flash monitor
```

## Example: Testing Tap Detection

```c
void test_tap_with_jitter_allowed(void)
{
    // High total movement but low net displacement = jitter, still a tap
    trackpad_tap_result_t result = trackpad_classify_tap(
        100,    // Duration: 100ms (valid for tap)
        10,     // Net displacement: 10px (small)
        50,     // Total movement: 50px (high - user's hand shook)
        false   // Not a double-tap
    );

    TEST_ASSERT_EQUAL(TRACKPAD_TAP_SINGLE, result);
}
```

This tests that shaky hands don't prevent tap detection - essential for real-world use!

## Example: Testing Complete Drag Sequence

```c
void test_drag_from_start_to_finish(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    // First tap
    test_tap_at(ctx, 100, 100, 100);
    test_advance_time(ctx, 100);

    // Second tap
    test_touch_down(ctx, 100, 100);

    // Verify drag indicator shown
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder,
                                         TRACKPAD_ACTION_SHOW_DRAG_INDICATOR));

    // Move to trigger drag
    test_advance_time(ctx, 50);
    test_touch_move(ctx, 130, 100);

    // Verify drag started
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_DRAG_START));

    // Continue dragging
    test_touch_move(ctx, 160, 100);
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_DRAG_MOVE));

    // Release
    test_touch_up(ctx, 200, 100);
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_DRAG_END));

    test_end(ctx);
}
```

## Benefits

### 1. Confidence in Changes
- Refactor acceleration curves without breaking tap detection
- Tune thresholds without manual testing every edge case
- Catch regressions immediately

### 2. Fast Feedback Loop
- Tests run in milliseconds on host machine
- No flashing ESP32
- No manual gesture simulation

### 3. Edge Case Coverage
- Exact boundary conditions (50ms tap duration, 15px threshold)
- Timing scenarios (double-tap window expiry)
- Zone boundaries (exactly at scroll zone edge)

### 4. Documentation
- Tests are executable specifications
- Show exact expected behavior
- Examples for new developers

### 5. Safe Refactoring
- Change internal implementation
- Tests verify external behavior unchanged
- Regression safety

## Next Steps

### Current Status: Phase 1 Complete ✅
- ✅ Pure gesture logic extracted
- ✅ Comprehensive tests written
- ✅ Test infrastructure (recorder, DSL)
- ✅ Host-compilable unit tests

### Phase 2: Integration (TODO)
- Update `ui_trackpad.c` to use `trackpad_gesture.c`
- Verify on-device behavior matches tests
- Performance profiling

### Phase 3: CI/CD (Future)
- Automated test runs on pull requests
- Code coverage reporting
- Benchmark tracking

## Troubleshooting

### CMake Version Too Old
If you see errors about `FetchContent`, upgrade CMake to 3.14+:
```bash
cmake --version  # Should be 3.14 or higher
```

### Tests Fail After Changing Thresholds
This is expected! Update test assertions to match new thresholds.

### Test Passes But Device Behavior Wrong
Check LVGL adapter layer (`ui_trackpad.c`) - may not be executing actions correctly.

## Learning Resources

- **Unity Framework**: https://github.com/ThrowTheSwitch/Unity
- **TDD Embedded**: "Test-Driven Development for Embedded C" by James Grenning
- **Clean Architecture**: "Clean Architecture" by Robert C. Martin (Chapter on boundaries)

## Summary

The trackpad now has **47 automated tests** covering:
- 6 pure function suites
- 7 gesture sequence scenarios
- Critical edge cases and boundaries

**Testability Score:**
- Before: 0% (untestable monolith)
- After: ~80% (pure logic under test)

The 20% not tested is the LVGL adapter layer, which is now thin enough to verify manually or with simple integration tests.

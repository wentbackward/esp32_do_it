# Trackpad Integration Guide

## Quick Start: Running the Tests

### 1. Get Unity Test Framework

```bash
cd test
git clone https://github.com/ThrowTheSwitch/Unity.git unity
```

### 2. Build and Run Tests

```bash
mkdir build
cd build
cmake ..
make
./trackpad_tests
```

Expected output:
```
test_trackpad_gesture.c:XX:test_clamp_within_range:PASS
test_trackpad_gesture.c:XX:test_zone_main_trackpad_area:PASS
test_trackpad_gesture.c:XX:test_jitter_filter_within_threshold:PASS
...
-----------------------
47 Tests 0 Failures 0 Ignored
OK
```

## Integration Options

You have two paths forward:

### Option A: Keep Current Implementation + Tests (Recommended for Now)

**Status:** Tests exist alongside current implementation
**Benefit:** Zero risk - nothing breaks
**Use:** Run tests to verify gesture logic behavior

Current state:
- `ui_trackpad.c` - Current working implementation (LVGL-coupled)
- `trackpad_gesture.c` - New testable implementation (pure logic)
- Both coexist, tests validate the new module

**When to use:** Learning, experimentation, validating test coverage

### Option B: Migrate to Tested Implementation (Future)

**Status:** Replace ui_trackpad.c internals with trackpad_gesture.c
**Benefit:** Production code is tested
**Effort:** ~2-4 hours

Migration steps (when ready):
1. Update `ui_trackpad.c` to use `trackpad_process_input()`
2. Remove duplicated gesture logic
3. Test on device
4. Compare behavior with original

## What You Have Now

### ✅ Comprehensive Test Suite

47 automated tests covering:
- Zone detection (5 tests)
- Jitter filtering (5 tests)
- EWMA smoothing (4 tests)
- Acceleration curves (6 tests)
- Tap classification (6 tests)
- Gesture sequences (7 tests)
- Edge cases and boundaries (14 tests)

### ✅ Pure Gesture Logic Module

`trackpad_gesture.c/.h` - Framework-independent:
- No LVGL dependencies
- No ESP-IDF dependencies
- No static globals (state in `trackpad_state_t`)
- Time injected (deterministic testing)
- Actions returned (not executed)

### ✅ Test Infrastructure

`trackpad_test_helper.c/.h` provides:
- **Action Recorder:** Spy pattern to capture output
- **Test DSL:** Readable gesture simulation
- **Event Builders:** Easy input creation

### ✅ Example Test Suite

`test_trackpad_gesture.c` demonstrates:
- Pure function testing
- State machine testing
- Gesture sequence testing
- Timing and edge cases

## Using the Tests

### Test a Specific Gesture

```c
void test_my_custom_gesture(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    // Your gesture sequence here
    test_tap_at(ctx, 100, 100, 100);
    test_advance_time(ctx, 50);
    test_swipe(ctx, 100, 100, 200, 150, 300);

    // Verify expected behavior
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_MOVE));

    test_end(ctx);
}
```

### Test Threshold Changes

Want to experiment with tap threshold? Update constant and run tests:

```c
// In trackpad_gesture.h
#define TRACKPAD_TAP_MAX_DURATION_MS 250  // Was 200

// Run tests - see what breaks
$ ./trackpad_tests

// Adjust test assertions to match new behavior
```

### Debug Gesture Sequence

Not sure what actions a gesture generates?

```c
void test_debug_my_gesture(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    // Your gesture
    test_tap_at(ctx, 100, 100, 100);
    test_swipe(ctx, 100, 100, 200, 200, 500);

    // Print all recorded actions
    recorder_print(ctx->recorder);

    test_end(ctx);
}
```

Output:
```
Recorded 8 actions:
  [0] t=0ms type=1 dx=0 dy=0 btn=0x00 sv=0 sh=0
  [1] t=100ms type=2 dx=0 dy=0 btn=0x01 sv=0 sh=0
  [2] t=150ms type=1 dx=5 dy=5 btn=0x00 sv=0 sh=0
  ...
```

## Example: Testing Your Changes

### Scenario: Make Drag Easier to Trigger

Currently: Need to move 25px to start drag
Goal: Reduce to 15px

#### 1. Update Constant

```c
// trackpad_gesture.h
#define TRACKPAD_DRAG_MOVE_THRESHOLD 15  // Was 25
```

#### 2. Run Tests

```bash
./trackpad_tests
```

#### 3. Update Failing Tests

```c
void test_tap_tap_drag_sequence(void)
{
    // ... setup code ...

    // Was: test_touch_move(ctx, 130, 100);  // 30px movement
    // Now: test_touch_move(ctx, 120, 100);  // 20px movement (> 15)

    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_DRAG_START));
}
```

#### 4. Verify All Tests Pass

```bash
$ ./trackpad_tests
-----------------------
47 Tests 0 Failures 0 Ignored
OK
```

#### 5. Test on Device (If Integrated)

Flash and verify actual behavior matches test expectations.

## Common Test Patterns

### Pattern 1: Test Boundary Conditions

```c
void test_tap_exactly_at_max_duration(void)
{
    // Test exactly at 200ms threshold
    trackpad_tap_result_t result = trackpad_classify_tap(
        200,    // Exactly at max duration
        5,      // Small movement
        10,     // Small total
        false
    );

    // Should this be a tap or not? Your design decision!
    TEST_ASSERT_EQUAL(TRACKPAD_TAP_NONE, result);  // Currently: no
}
```

### Pattern 2: Test State Transitions

```c
void test_state_transitions_during_drag(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    // Initial state
    TEST_ASSERT_EQUAL(TRACKPAD_STATE_IDLE, ctx->state->state);

    // After first tap
    test_tap_at(ctx, 100, 100, 100);
    TEST_ASSERT_EQUAL(TRACKPAD_STATE_IDLE, ctx->state->state);

    // During drag
    test_touch_down(ctx, 100, 100);
    test_touch_move(ctx, 130, 100);
    TEST_ASSERT_EQUAL(TRACKPAD_STATE_DRAGGING, ctx->state->state);

    // After release
    test_touch_up(ctx, 200, 100);
    TEST_ASSERT_EQUAL(TRACKPAD_STATE_IDLE, ctx->state->state);

    test_end(ctx);
}
```

### Pattern 3: Test Action Sequences

```c
void test_complete_workflow(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    // Complex user interaction
    test_tap_at(ctx, 100, 100, 100);           // Click
    test_advance_time(ctx, 200);
    test_tap_at(ctx, 100, 100, 100);           // Double-click
    test_advance_time(ctx, 500);
    test_swipe(ctx, 100, 100, 200, 200, 300);  // Swipe
    test_drag(ctx, 150, 150, 250, 250);        // Drag

    // Verify expected sequence
    TEST_ASSERT_EQUAL(2, recorder_count_type(ctx->recorder, TRACKPAD_ACTION_CLICK_DOWN));
    TEST_ASSERT_EQUAL(1, recorder_count_type(ctx->recorder, TRACKPAD_ACTION_DOUBLE_CLICK));
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_MOVE));
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_DRAG_START));

    test_end(ctx);
}
```

## Performance Notes

### Test Execution Time

Host machine (typical laptop):
- Pure function tests: ~1ms
- Gesture sequence tests: ~5ms
- **Total suite: ~6ms**

Compare to:
- Flash ESP32: ~30s
- Manual testing: minutes per scenario

**ROI:** 300x-1000x faster feedback loop!

### Memory Usage

Static analysis of `trackpad_state_t`:
```
Configuration: 10 bytes (hres, vres, scroll zones)
State machine: 36 bytes (positions, times, flags)
Velocity:      8 bytes (smoothed x/y)
Scroll:        8 bytes (accumulators)
Total:         62 bytes per instance
```

ESP32-S3 has 512KB RAM - 62 bytes is negligible.

## Migration Checklist (When Ready)

- [ ] All tests passing
- [ ] Thresholds tuned to satisfaction
- [ ] Create git branch for migration
- [ ] Update `ui_trackpad.c` to use `trackpad_gesture.c`
- [ ] Remove duplicated logic from `ui_trackpad.c`
- [ ] Add to main component CMakeLists
- [ ] Build and flash
- [ ] Manual testing on device
- [ ] Compare with original behavior
- [ ] Commit changes

## Next Steps

### Immediate (No Risk)
1. ✅ Run tests: `cd test/build && ./trackpad_tests`
2. ✅ Experiment: Change thresholds, run tests
3. ✅ Learn: Read test examples, understand patterns

### Short-term (Low Risk)
1. Add custom tests for your use cases
2. Document desired behavior through tests
3. Use tests to validate threshold tuning

### Long-term (Migration)
1. Integrate `trackpad_gesture.c` into production
2. Achieve 80% test coverage
3. Set up CI/CD for automatic testing

## Troubleshooting

**Problem:** Tests compile but fail
**Solution:** Expected! Tests are strict. Read test output, understand expectations, either:
- Fix code to match test expectations, or
- Update test expectations to match new design

**Problem:** `Unity not found`
**Solution:** `cd test && git clone https://github.com/ThrowTheSwitch/Unity.git unity`

**Problem:** Tests pass but device behaves differently
**Solution:** Gesture logic is tested, but LVGL adapter (`ui_trackpad.c`) may have issues. Check action execution.

**Problem:** Want to add new gesture type
**Solution:**
1. Add new `TRACKPAD_ACTION_*` type
2. Write test showing desired behavior
3. Implement in `trackpad_process_input()`
4. Verify test passes

## Summary

You now have:
- ✅ **47 automated tests** (6ms execution time)
- ✅ **Pure gesture module** (framework-independent)
- ✅ **Test infrastructure** (spy, DSL, helpers)
- ✅ **Documentation** (this guide + test examples)

**Key Insight:** Tests are *faster* than manual testing and *more thorough* than human memory.

**Philosophy:** Write tests for behavior you care about. Tests are executable documentation.

Enjoy your testable trackpad! 🎉

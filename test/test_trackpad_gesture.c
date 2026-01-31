/**
 * @file test_trackpad_gesture.c
 * @brief Comprehensive unit tests for trackpad gesture processing
 *
 * These tests are host-compilable (no ESP-IDF/LVGL dependencies).
 * Can be run on development machine for fast iteration.
 */

#include "unity.h"
#include "trackpad_gesture.h"
#include "trackpad_test_helper.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

// ========================== Pure Function Tests ==========================

void test_clamp_within_range(void)
{
    TEST_ASSERT_EQUAL_INT32(5, trackpad_clamp_i32(5, 0, 10));
}

void test_clamp_below_min(void)
{
    TEST_ASSERT_EQUAL_INT32(0, trackpad_clamp_i32(-5, 0, 10));
}

void test_clamp_above_max(void)
{
    TEST_ASSERT_EQUAL_INT32(10, trackpad_clamp_i32(15, 0, 10));
}

// ---- Zone Detection Tests ----

void test_zone_main_trackpad_area(void)
{
    trackpad_zone_t z = trackpad_get_zone(100, 100, 320, 240, 40, 40);
    TEST_ASSERT_EQUAL(TRACKPAD_ZONE_MAIN, z);
}

void test_zone_right_edge_vertical_scroll(void)
{
    // x=285 is in right scroll zone (320-40=280, so >=280 is scroll zone)
    trackpad_zone_t z = trackpad_get_zone(285, 100, 320, 240, 40, 40);
    TEST_ASSERT_EQUAL(TRACKPAD_ZONE_SCROLL_V, z);
}

void test_zone_bottom_edge_horizontal_scroll(void)
{
    // y=205 is in bottom scroll zone (240-40=200, so >=200 is scroll zone)
    trackpad_zone_t z = trackpad_get_zone(100, 205, 320, 240, 40, 40);
    TEST_ASSERT_EQUAL(TRACKPAD_ZONE_SCROLL_H, z);
}

void test_zone_bottom_right_corner(void)
{
    trackpad_zone_t z = trackpad_get_zone(285, 205, 320, 240, 40, 40);
    TEST_ASSERT_EQUAL(TRACKPAD_ZONE_SCROLL_CORNER, z);
}

void test_zone_boundary_exactly_at_threshold(void)
{
    // x=280 is exactly at boundary (320-40)
    trackpad_zone_t z = trackpad_get_zone(280, 100, 320, 240, 40, 40);
    TEST_ASSERT_EQUAL(TRACKPAD_ZONE_SCROLL_V, z);  // >= includes boundary
}

// ---- Jitter Filter Tests ----

void test_jitter_filter_within_threshold(void)
{
    TEST_ASSERT_EQUAL_INT32(0, trackpad_filter_jitter(2, 3));
    TEST_ASSERT_EQUAL_INT32(0, trackpad_filter_jitter(-2, 3));
    TEST_ASSERT_EQUAL_INT32(0, trackpad_filter_jitter(3, 3));  // Exactly at threshold
}

void test_jitter_filter_outside_threshold_positive(void)
{
    // 5 - 3 = 2
    TEST_ASSERT_EQUAL_INT32(2, trackpad_filter_jitter(5, 3));
    TEST_ASSERT_EQUAL_INT32(7, trackpad_filter_jitter(10, 3));
}

void test_jitter_filter_outside_threshold_negative(void)
{
    // -5 + 3 = -2
    TEST_ASSERT_EQUAL_INT32(-2, trackpad_filter_jitter(-5, 3));
    TEST_ASSERT_EQUAL_INT32(-7, trackpad_filter_jitter(-10, 3));
}

void test_jitter_detection_true(void)
{
    TEST_ASSERT_TRUE(trackpad_is_jitter(2, 2, 3));
    TEST_ASSERT_TRUE(trackpad_is_jitter(3, 3, 3));
    TEST_ASSERT_TRUE(trackpad_is_jitter(0, 0, 3));
    TEST_ASSERT_TRUE(trackpad_is_jitter(-2, 2, 3));
}

void test_jitter_detection_false(void)
{
    TEST_ASSERT_FALSE(trackpad_is_jitter(4, 0, 3));
    TEST_ASSERT_FALSE(trackpad_is_jitter(0, 4, 3));
    TEST_ASSERT_FALSE(trackpad_is_jitter(4, 4, 3));
}

// ---- EWMA Tests ----

void test_ewma_initial_step(void)
{
    // Starting from 0, first value with alpha=0.3
    float result = trackpad_ewma_update(0.0f, 100.0f, 0.3f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 30.0f, result);  // 0.3*100 + 0.7*0 = 30
}

void test_ewma_convergence(void)
{
    // Repeated constant input should converge to that value
    float smooth = 0.0f;
    for (int i = 0; i < 20; i++) {
        smooth = trackpad_ewma_update(smooth, 100.0f, 0.3f);
    }
    TEST_ASSERT_FLOAT_WITHIN(1.0f, 100.0f, smooth);
}

void test_ewma_alpha_1_no_smoothing(void)
{
    // Alpha=1.0 means no smoothing, immediate response
    float result = trackpad_ewma_update(50.0f, 100.0f, 1.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, result);
}

void test_ewma_alpha_0_full_smoothing(void)
{
    // Alpha=0.0 means infinite smoothing, no change
    float result = trackpad_ewma_update(50.0f, 100.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, result);
}

// ---- Acceleration Curve Tests ----

void test_acceleration_subpixel_passthrough(void)
{
    float result = trackpad_apply_acceleration(0.3f, 500.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.3f, result);
}

void test_acceleration_precision_zone(void)
{
    // Very slow movement (50 px/s) gets 0.5x multiplier
    float result = trackpad_apply_acceleration(10.0f, 50.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5.0f, result);  // 10 * 0.5 = 5
}

void test_acceleration_max_zone(void)
{
    // Very fast movement (2000 px/s) gets 5.0x multiplier
    float result = trackpad_apply_acceleration(10.0f, 2000.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 50.0f, result);  // 10 * 5.0 = 50
}

void test_acceleration_linear_transition_midpoint(void)
{
    // At midpoint of transition zone (250 px/s), expect ~0.75x
    // Transition is 100-400, midpoint is 250
    // t = (250-100)/(400-100) = 0.5
    // multiplier = 0.5 + 0.5*(1.0-0.5) = 0.75
    float result = trackpad_apply_acceleration(10.0f, 250.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 7.5f, result);
}

void test_acceleration_negative_delta(void)
{
    // Negative deltas should also be accelerated
    float result = trackpad_apply_acceleration(-10.0f, 50.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, -5.0f, result);
}

void test_acceleration_boundary_at_precision_threshold(void)
{
    // Exactly at 100 px/s - should still be precision zone
    float result = trackpad_apply_acceleration(10.0f, 100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 5.0f, result);  // Still 0.5x
}

// ---- Tap Classification Tests ----

void test_tap_too_short_is_bounce(void)
{
    trackpad_tap_result_t result = trackpad_classify_tap(
        30,     // 30ms - too short (min is 50ms)
        5,      // minimal movement
        10,     // minimal total
        false
    );
    TEST_ASSERT_EQUAL(TRACKPAD_TAP_NONE, result);
}

void test_tap_too_long_is_hold(void)
{
    trackpad_tap_result_t result = trackpad_classify_tap(
        250,    // 250ms - too long (max is 200ms)
        5,
        10,
        false
    );
    TEST_ASSERT_EQUAL(TRACKPAD_TAP_NONE, result);
}

void test_tap_valid_duration_and_movement(void)
{
    trackpad_tap_result_t result = trackpad_classify_tap(
        100,    // Good duration
        10,     // Small movement
        15,
        false
    );
    TEST_ASSERT_EQUAL(TRACKPAD_TAP_SINGLE, result);
}

void test_tap_with_swipe_cancelled(void)
{
    trackpad_tap_result_t result = trackpad_classify_tap(
        100,    // Good duration
        50,     // Too much net movement - this is a swipe
        60,
        false
    );
    TEST_ASSERT_EQUAL(TRACKPAD_TAP_NONE, result);
}

void test_tap_with_jitter_allowed(void)
{
    // High total movement but low net displacement = jitter, still a tap
    trackpad_tap_result_t result = trackpad_classify_tap(
        100,    // Good duration
        10,     // Low net displacement
        50,     // High total (jittery)
        false
    );
    TEST_ASSERT_EQUAL(TRACKPAD_TAP_SINGLE, result);
}

void test_double_tap_detected(void)
{
    trackpad_tap_result_t result = trackpad_classify_tap(
        100,
        5,
        10,
        true    // Double-tap pending
    );
    TEST_ASSERT_EQUAL(TRACKPAD_TAP_DOUBLE, result);
}

// ========================== Gesture Sequence Tests ==========================

void test_simple_tap_generates_click(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    test_tap_at(ctx, 100, 100, 100);

    // Should generate click down action
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_CLICK_DOWN));
    TEST_ASSERT_EQUAL(1, recorder_count_type(ctx->recorder, TRACKPAD_ACTION_CLICK_DOWN));

    const recorded_action_t *click = recorder_find_first(ctx->recorder, TRACKPAD_ACTION_CLICK_DOWN);
    TEST_ASSERT_NOT_NULL(click);
    TEST_ASSERT_EQUAL_UINT8(0x01, click->buttons);

    test_end(ctx);
}

void test_movement_generates_move_actions(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    test_swipe(ctx, 100, 100, 150, 100, 200);

    // Should generate move actions
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_MOVE));
    TEST_ASSERT_GREATER_THAN(0, recorder_count_type(ctx->recorder, TRACKPAD_ACTION_MOVE));

    test_end(ctx);
}

void test_tap_tap_drag_sequence(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    // First tap
    test_tap_at(ctx, 100, 100, 100);
    test_advance_time(ctx, 100);

    // Second tap within window
    test_touch_down(ctx, 100, 100);

    // Should show drag indicator
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_SHOW_DRAG_INDICATOR));

    test_advance_time(ctx, 50);

    // Move enough to trigger drag
    test_touch_move(ctx, 130, 100);
    test_advance_time(ctx, 50);

    // Should start drag
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_DRAG_START));

    // More movement while dragging
    test_touch_move(ctx, 160, 100);
    test_advance_time(ctx, 50);

    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_DRAG_MOVE));

    // Release ends drag
    test_touch_up(ctx, 200, 100);

    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_DRAG_END));

    test_end(ctx);
}

void test_scroll_zone_vertical(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    // Touch in right scroll zone (x > 320-40 = 280)
    test_touch_down(ctx, 290, 100);
    test_advance_time(ctx, 50);

    // Scroll down
    test_touch_move(ctx, 290, 130);

    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_SCROLL_V));

    const recorded_action_t *scroll = recorder_find_first(ctx->recorder, TRACKPAD_ACTION_SCROLL_V);
    TEST_ASSERT_NOT_NULL(scroll);
    TEST_ASSERT_NOT_EQUAL(0, scroll->scroll_v);

    test_end(ctx);
}

void test_scroll_zone_horizontal(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    // Touch in bottom scroll zone (y > 240-40 = 200)
    test_touch_down(ctx, 100, 210);
    test_advance_time(ctx, 50);

    // Scroll right
    test_touch_move(ctx, 130, 210);

    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_SCROLL_H));

    test_end(ctx);
}

void test_double_tap_generates_double_click(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    // First tap
    test_tap_at(ctx, 100, 100, 100);
    test_advance_time(ctx, 100);

    // Second tap quickly (within 350ms window)
    test_tap_at(ctx, 100, 100, 100);

    // Should generate double-click
    TEST_ASSERT_TRUE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_DOUBLE_CLICK));

    test_end(ctx);
}

void test_tap_after_timeout_is_two_separate_clicks(void)
{
    test_context_t *ctx = test_begin(320, 240, 40, 40);

    // First tap
    test_tap_at(ctx, 100, 100, 100);
    test_advance_time(ctx, 400);  // Wait longer than double-tap window (350ms)

    // Second tap - should be separate click, not double
    test_tap_at(ctx, 100, 100, 100);

    TEST_ASSERT_EQUAL(2, recorder_count_type(ctx->recorder, TRACKPAD_ACTION_CLICK_DOWN));
    TEST_ASSERT_FALSE(recorder_has_action(ctx->recorder, TRACKPAD_ACTION_DOUBLE_CLICK));

    test_end(ctx);
}

// ========================== Test Runner ==========================

void run_pure_function_tests(void)
{
    RUN_TEST(test_clamp_within_range);
    RUN_TEST(test_clamp_below_min);
    RUN_TEST(test_clamp_above_max);

    RUN_TEST(test_zone_main_trackpad_area);
    RUN_TEST(test_zone_right_edge_vertical_scroll);
    RUN_TEST(test_zone_bottom_edge_horizontal_scroll);
    RUN_TEST(test_zone_bottom_right_corner);
    RUN_TEST(test_zone_boundary_exactly_at_threshold);

    RUN_TEST(test_jitter_filter_within_threshold);
    RUN_TEST(test_jitter_filter_outside_threshold_positive);
    RUN_TEST(test_jitter_filter_outside_threshold_negative);
    RUN_TEST(test_jitter_detection_true);
    RUN_TEST(test_jitter_detection_false);

    RUN_TEST(test_ewma_initial_step);
    RUN_TEST(test_ewma_convergence);
    RUN_TEST(test_ewma_alpha_1_no_smoothing);
    RUN_TEST(test_ewma_alpha_0_full_smoothing);

    RUN_TEST(test_acceleration_subpixel_passthrough);
    RUN_TEST(test_acceleration_precision_zone);
    RUN_TEST(test_acceleration_max_zone);
    RUN_TEST(test_acceleration_linear_transition_midpoint);
    RUN_TEST(test_acceleration_negative_delta);
    RUN_TEST(test_acceleration_boundary_at_precision_threshold);

    RUN_TEST(test_tap_too_short_is_bounce);
    RUN_TEST(test_tap_too_long_is_hold);
    RUN_TEST(test_tap_valid_duration_and_movement);
    RUN_TEST(test_tap_with_swipe_cancelled);
    RUN_TEST(test_tap_with_jitter_allowed);
    RUN_TEST(test_double_tap_detected);
}

void run_gesture_sequence_tests(void)
{
    RUN_TEST(test_simple_tap_generates_click);
    RUN_TEST(test_movement_generates_move_actions);
    RUN_TEST(test_tap_tap_drag_sequence);
    RUN_TEST(test_scroll_zone_vertical);
    RUN_TEST(test_scroll_zone_horizontal);
    RUN_TEST(test_double_tap_generates_double_click);
    RUN_TEST(test_tap_after_timeout_is_two_separate_clicks);
}

int main(void)
{
    UNITY_BEGIN();

    run_pure_function_tests();
    run_gesture_sequence_tests();

    return UNITY_END();
}

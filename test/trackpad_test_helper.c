/**
 * @file trackpad_test_helper.c
 * @brief Test infrastructure implementation
 */

#include "trackpad_test_helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ========================== Action Recorder ==========================

void recorder_reset(action_recorder_t *rec)
{
    memset(rec, 0, sizeof(*rec));
}

void recorder_add(action_recorder_t *rec, const trackpad_action_t *action, uint32_t timestamp)
{
    if (rec->count < MAX_RECORDED_ACTIONS) {
        recorded_action_t *r = &rec->actions[rec->count++];
        r->type = action->type;
        r->dx = action->dx;
        r->dy = action->dy;
        r->buttons = action->buttons;
        r->scroll_v = action->scroll_v;
        r->scroll_h = action->scroll_h;
        r->timestamp = timestamp;
    }
}

bool recorder_has_action(const action_recorder_t *rec, trackpad_action_type_t type)
{
    for (int i = 0; i < rec->count; i++) {
        if (rec->actions[i].type == type) {
            return true;
        }
    }
    return false;
}

int recorder_count_type(const action_recorder_t *rec, trackpad_action_type_t type)
{
    int count = 0;
    for (int i = 0; i < rec->count; i++) {
        if (rec->actions[i].type == type) {
            count++;
        }
    }
    return count;
}

const recorded_action_t* recorder_find_first(const action_recorder_t *rec,
                                             trackpad_action_type_t type)
{
    for (int i = 0; i < rec->count; i++) {
        if (rec->actions[i].type == type) {
            return &rec->actions[i];
        }
    }
    return NULL;
}

void recorder_print(const action_recorder_t *rec)
{
    printf("Recorded %d actions:\n", rec->count);
    for (int i = 0; i < rec->count; i++) {
        const recorded_action_t *a = &rec->actions[i];
        printf("  [%d] t=%ums type=%d dx=%d dy=%d btn=0x%02x sv=%d sh=%d\n",
               i, a->timestamp, a->type, a->dx, a->dy, a->buttons, a->scroll_v, a->scroll_h);
    }
}

// ========================== Test Context Builder ==========================

test_context_t* test_begin(uint16_t hres, uint16_t vres,
                           int32_t scroll_w, int32_t scroll_h)
{
    test_context_t *ctx = (test_context_t*)calloc(1, sizeof(test_context_t));
    ctx->state = (trackpad_state_t*)calloc(1, sizeof(trackpad_state_t));
    ctx->recorder = (action_recorder_t*)calloc(1, sizeof(action_recorder_t));

    trackpad_state_init(ctx->state, hres, vres, scroll_w, scroll_h);
    recorder_reset(ctx->recorder);
    ctx->current_time = 0;

    return ctx;
}

void test_end(test_context_t *ctx)
{
    if (ctx) {
        free(ctx->state);
        free(ctx->recorder);
        free(ctx);
    }
}

void test_touch_down(test_context_t *ctx, int32_t x, int32_t y)
{
    trackpad_input_t input = make_pressed_event(x, y, ctx->current_time);
    trackpad_action_t action;

    if (trackpad_process_input(ctx->state, &input, &action)) {
        recorder_add(ctx->recorder, &action, ctx->current_time);
    }
}

void test_touch_move(test_context_t *ctx, int32_t x, int32_t y)
{
    trackpad_input_t input = make_pressing_event(x, y, ctx->current_time);
    trackpad_action_t action;

    if (trackpad_process_input(ctx->state, &input, &action)) {
        recorder_add(ctx->recorder, &action, ctx->current_time);
    }
}

void test_touch_up(test_context_t *ctx, int32_t x, int32_t y)
{
    trackpad_input_t input = make_released_event(x, y, ctx->current_time);
    trackpad_action_t action;

    if (trackpad_process_input(ctx->state, &input, &action)) {
        recorder_add(ctx->recorder, &action, ctx->current_time);
    }
}

void test_advance_time(test_context_t *ctx, uint32_t ms)
{
    ctx->current_time += ms;
}

void test_tap_at(test_context_t *ctx, int32_t x, int32_t y, uint32_t duration_ms)
{
    test_touch_down(ctx, x, y);
    test_advance_time(ctx, duration_ms);
    test_touch_up(ctx, x, y);
}

void test_swipe(test_context_t *ctx, int32_t x1, int32_t y1,
                int32_t x2, int32_t y2, uint32_t duration_ms)
{
    test_touch_down(ctx, x1, y1);

    // Simulate 10 intermediate points
    int steps = 10;
    uint32_t step_time = duration_ms / steps;

    for (int i = 1; i <= steps; i++) {
        test_advance_time(ctx, step_time);
        int32_t x = x1 + (x2 - x1) * i / steps;
        int32_t y = y1 + (y2 - y1) * i / steps;
        test_touch_move(ctx, x, y);
    }

    test_touch_up(ctx, x2, y2);
}

void test_drag(test_context_t *ctx, int32_t x1, int32_t y1,
               int32_t x2, int32_t y2)
{
    // First tap
    test_tap_at(ctx, x1, y1, 100);
    test_advance_time(ctx, 100);

    // Second tap + drag
    test_touch_down(ctx, x1, y1);
    test_advance_time(ctx, 50);
    test_touch_move(ctx, x1 + 10, y1);  // Move to trigger drag
    test_advance_time(ctx, 50);

    // Drag to destination
    test_swipe(ctx, x1 + 10, y1, x2, y2, 200);
}

// ========================== Input Event Helpers ==========================

trackpad_input_t make_pressed_event(int32_t x, int32_t y, uint32_t time)
{
    trackpad_input_t input = {
        .type = TRACKPAD_EVENT_PRESSED,
        .x = x,
        .y = y,
        .timestamp_ms = time
    };
    return input;
}

trackpad_input_t make_pressing_event(int32_t x, int32_t y, uint32_t time)
{
    trackpad_input_t input = {
        .type = TRACKPAD_EVENT_PRESSING,
        .x = x,
        .y = y,
        .timestamp_ms = time
    };
    return input;
}

trackpad_input_t make_released_event(int32_t x, int32_t y, uint32_t time)
{
    trackpad_input_t input = {
        .type = TRACKPAD_EVENT_RELEASED,
        .x = x,
        .y = y,
        .timestamp_ms = time
    };
    return input;
}

/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "layout_screen.h"
#include "display_mux.h"
#include "hid_device_keyboard.h"
#include "hid_device_mouse.h"
#include "esp_log.h"

static const char *TAG = "LayoutScreen";
#define TOUCH_POINT_MAX (5)

typedef struct {
    const layout_input_t *input;
    uint8_t touched;
    union {
        struct {
            bool moved;
        } trackpad;
    };
} active_input_state_t;

// MARK: Key
static void key_touch_press(active_input_state_t *state, uint8_t track_id, uint16_t x, uint16_t y) {
    hid_device_keyboard_press_key(state->input->key);
    display_mux_layout_draw_region(display_mux_layout_active_image,
        state->input->region.x, state->input->region.y, state->input->region.width, state->input->region.height);
}
static void key_touch_release(active_input_state_t *state, uint8_t track_id) {
    hid_device_keyboard_release_key(state->input->key);
    display_mux_layout_draw_region(display_mux_layout_base_image,
        state->input->region.x, state->input->region.y, state->input->region.width, state->input->region.height);
}

// MARK: Trackpad
static void trackpad_touch_press(active_input_state_t *state, uint8_t track_id, uint16_t x, uint16_t y) {
    state->trackpad.moved = false;
}
static void trackpad_touch_move(active_input_state_t *state, uint8_t track_id, uint16_t x, uint16_t y, int16_t dx, int16_t dy) {
    state->trackpad.moved = true;
    hid_device_mouse_move(dx, dy);
}

// MARK: Touch Handles
static const layout_config_t *current_layout_config;
static active_input_state_t active_input_states[TOUCH_POINT_MAX];
static struct {
    bool touched;
    esp_lcd_touch_point_data_t data;
} last_touch_points[TOUCH_POINT_MAX];

static const struct {
    void (*press)(active_input_state_t *state, uint8_t track_id, uint16_t x, uint16_t y);
    void (*add)(active_input_state_t *state, uint8_t track_id, uint16_t x, uint16_t y);
    void (*move)(active_input_state_t *state, uint8_t track_id, uint16_t x, uint16_t y, int16_t dx, int16_t dy);
    void (*remove)(active_input_state_t *state, uint8_t track_id);
    void (*release)(active_input_state_t *state, uint8_t track_id);
} touch_callback[LAYOUT_INPUT_TYPE_MAX] = {
    [LAYOUT_INPUT_TYPE_KEY] = {
        .press = key_touch_press,
        .release = key_touch_release,
    },
    [LAYOUT_INPUT_TYPE_TRACKPAD] = {
        .press = trackpad_touch_press,
        .move = trackpad_touch_move,
    },
};

#define GET_CALLBACK(state) (touch_callback[state->input->type])
static void invoke_callback_press(active_input_state_t *state, esp_lcd_touch_point_data_t *point) {
    ESP_LOGI(TAG, "Press: [%d] x=%d, y=%d", point->track_id, point->x, point->y);
    if (GET_CALLBACK(state).press) {
        GET_CALLBACK(state).press(state, point->track_id, point->x, point->y);
    }
}
static void invoke_callback_add(active_input_state_t *state, esp_lcd_touch_point_data_t *point) {
    ESP_LOGI(TAG, "Add: [%d] x=%d, y=%d", point->track_id, point->x, point->y);
    if (GET_CALLBACK(state).add) {
        GET_CALLBACK(state).add(state, point->track_id, point->x, point->y);
    }
}
static void invoke_callback_move(active_input_state_t *state, esp_lcd_touch_point_data_t *point) {
    int16_t dx = point->x - last_touch_points[point->track_id].data.x;
    int16_t dy = point->y - last_touch_points[point->track_id].data.y;
    if (dx == 0 && dy == 0) return;
    ESP_LOGI(TAG, "Move: [%d] x=%d, y=%d, dx=%d, dy=%d", point->track_id, point->x, point->y, dx, dy);
    if (GET_CALLBACK(state).move) {
        GET_CALLBACK(state).move(state, point->track_id, point->x, point->y, dx, dy);
    }
}
static void invoke_callback_remove(active_input_state_t *state, uint8_t track_id) {
    ESP_LOGI(TAG, "Remove: [%d]", track_id);
    if (GET_CALLBACK(state).remove) {
        GET_CALLBACK(state).remove(state, track_id);
    }
}
static void invoke_callback_release(active_input_state_t *state, uint8_t track_id) {
    ESP_LOGI(TAG, "Release: [%d]", track_id);
    if (GET_CALLBACK(state).release) {
        GET_CALLBACK(state).release(state, track_id);
    }
}

static const layout_input_t *find_input(uint16_t x, uint16_t y) {
    for (int i = 0; i < current_layout_config->count; i++) {
        const layout_input_t *input = &current_layout_config->inputs[i];
        if (input->region.x <= x && input->region.x + input->region.width > x &&
            input->region.y <= y && input->region.y + input->region.height > y) {
            return input;
        }
    }
    return NULL;
}
static active_input_state_t *active_input_state_get(const layout_input_t* input) {
    for (int i = 0; i < ARRAY_SIZE(active_input_states); i++) {
        if (active_input_states[i].input == input) return &active_input_states[i];
    }
    return NULL;
}
static active_input_state_t *active_input_state_find(uint8_t track_id) {
    for (int i = 0; i < ARRAY_SIZE(active_input_states); i++) {
        if (!active_input_states[i].input) continue;
        if (active_input_states[i].touched & (1 << track_id)) return &active_input_states[i];
    }
    return NULL;
}

void layout_screen_on_touch(int touch_num, esp_lcd_touch_point_data_t touches[5]) {
    bool track_id_is_active[TOUCH_POINT_MAX] = {};
    for (int i = 0; i < touch_num; i++) {
        track_id_is_active[touches[i].track_id] = true;
        active_input_state_t *state = active_input_state_find(touches[i].track_id);
        if (state) {
            invoke_callback_move(state, &touches[i]);
            continue;
        }
        if (last_touch_points[touches[i].track_id].touched) continue;

        const layout_input_t *input = find_input(touches[i].x, touches[i].y);
        if (!input) continue;

        state = active_input_state_get(input);
        if (state) {
            state->touched |= (1 << touches[i].track_id);
            invoke_callback_add(state, &touches[i]);
            continue;
        }

        for (int i = 0; i < TOUCH_POINT_MAX; i++) {
            if (!active_input_states[i].input) {
                state = &active_input_states[i];
                break;
            }
        }
        state->input = input;
        state->touched = (1 << touches[i].track_id);
        invoke_callback_press(state, &touches[i]);
    }
    for (int i = 0; i < TOUCH_POINT_MAX; i++) {
        if (last_touch_points[i].touched && !track_id_is_active[i]) {
            active_input_state_t *state = active_input_state_find(i);
            if (state) {
                state->touched &= ~(1 << i);
                if (state->touched) {
                    invoke_callback_remove(state, i);
                } else {
                    invoke_callback_release(state, i);
                    state->input = NULL;
                }
            }
        }
        last_touch_points[i].touched = track_id_is_active[i];
        for (int j = 0; j < touch_num; j++) {
            if (touches[j].track_id == i) {
                last_touch_points[i].data = touches[j];
                break;
            }
        }
    }
}

void layout_screen_open(const layout_config_t *config) {
    current_layout_config = config;
    display_mux_layout_load_images(config->base_image, config->active_image);
    display_mux_switch_mode(DISPLAY_MUX_MODE_LAYOUT);
    display_mux_gui_screen_load(lv_obj_create(NULL));
}

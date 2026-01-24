/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "layout_screen.h"
#include "display_mux.h"

#define ACTIVE_INPUTS_MAX (5)
static const layout_config_t *current_layout_config;
static const layout_input_t *current_active_inputs[ACTIVE_INPUTS_MAX];

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
static bool input_is_active(const layout_input_t* input, const layout_input_t **list) {
    for (int i = 0; i < ACTIVE_INPUTS_MAX; i++) {
        if (list[i] == input) return true;
    }
    return false;
}

void layout_screen_on_touch(int touch_num, esp_lcd_touch_point_data_t touches[5]) {
    const layout_input_t *active_inputs[ACTIVE_INPUTS_MAX] = {};
    for (int i = 0; i < touch_num; i++) {
        active_inputs[touches[i].track_id] = find_input(touches[i].x, touches[i].y);
    }
    for (int i = 0; i < ACTIVE_INPUTS_MAX; i++) {
        if (!current_active_inputs[i] || input_is_active(current_active_inputs[i], active_inputs)) continue;
        display_mux_layout_draw_region(display_mux_layout_base_image,
            current_active_inputs[i]->region.x, current_active_inputs[i]->region.y,
            current_active_inputs[i]->region.width, current_active_inputs[i]->region.height
        );
    }
    for (int i = 0; i < ACTIVE_INPUTS_MAX; i++) {
        if (!active_inputs[i] || input_is_active(active_inputs[i], current_active_inputs)) continue;
        display_mux_layout_draw_region(display_mux_layout_active_image,
            active_inputs[i]->region.x, active_inputs[i]->region.y,
            active_inputs[i]->region.width, active_inputs[i]->region.height
        );
    }
    memcpy(current_active_inputs, active_inputs, sizeof(current_active_inputs));
}

void layout_screen_open(const layout_config_t *config) {
    current_layout_config = config;
    display_mux_layout_load_images(config->base_image, config->active_image);
    display_mux_switch_mode(DISPLAY_MUX_MODE_LAYOUT);
    display_mux_gui_screen_load(lv_obj_create(NULL));
}

/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#pragma once
#include "lvgl.h"
#include "layouts/layout.h"

typedef enum {
    DISPLAY_MUX_MODE_GUI,
    DISPLAY_MUX_MODE_LAYOUT,
} display_mux_mode_t;

// MARK: LVGL GUI
#define GUI_WIDTH  (640)
#define GUI_HEIGHT (360)
#define GUI_FB_NUM (2)

// MARK: Layout
void display_mux_layout_load_images(const layout_image_t *base, const layout_image_t *active);

// MARK: Common
void display_mux_switch_mode(display_mux_mode_t mode);
void display_mux_setup(void);

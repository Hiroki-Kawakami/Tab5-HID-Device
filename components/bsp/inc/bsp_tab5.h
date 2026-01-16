/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#pragma once
#include "bsp_common.h"

esp_err_t bsp_tab5_init(void);
void bsp_tab5_display_set_brightness(int brightness);
void *bsp_tab5_display_get_frame_buffer(int fb_index);
void bsp_tab5_display_flush(int fb_index);
int bsp_tab5_touch_read(bsp_point_t *points, uint8_t max_points);

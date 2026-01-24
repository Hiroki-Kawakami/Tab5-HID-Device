/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "layouts/layout.h"

void layout_screen_open(const layout_config_t *config);
void layout_screen_on_touch(int touch_num, esp_lcd_touch_point_data_t touches[5]);

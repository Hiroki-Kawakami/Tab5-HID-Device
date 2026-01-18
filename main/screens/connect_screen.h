/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "lvgl.h"

typedef struct {
    enum {
        CONNECT_SCREEN_MODE_CONNECT,
        CONNECT_SCREEN_MODE_PAIRING,
    } mode;
    const char *device_name;
    bool cancellable;
} connect_screen_config_t;

void connect_screen_open(lv_obj_t *screen, connect_screen_config_t *config);

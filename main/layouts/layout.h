/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#pragma once
#include "esp_lvgl_port.h"
#include "hid_device_mouse.h"
#include <string.h>

typedef struct {
    const uint8_t *data;
    size_t size;
} layout_image_t;

typedef enum {
    LAYOUT_INPUT_TYPE_NONE,
    LAYOUT_INPUT_TYPE_KEY,
    LAYOUT_INPUT_TYPE_MOUSE_BUTTON,
    LAYOUT_INPUT_TYPE_TRACKPAD,
    LAYOUT_INPUT_TYPE_MAX,
} layout_input_type_t;

typedef struct {
    layout_input_type_t type;
    struct {
        uint16_t x, y, width, height;
    } region;
    union {
        uint32_t key;
        hid_device_mouse_button_t mouse_button;
    };
} layout_input_t;

typedef struct {
    const char *title;
    const layout_image_t *base_image;
    const layout_image_t *active_image;
    const layout_input_t *inputs;
    size_t count;
} layout_config_t;

typedef struct _layout_context _layout_context_t;
struct _layout_context {
    const layout_config_t *config;
    _layout_context_t *next;
};
extern _layout_context_t *_layout_head;
#define LAYOUT_REGISTER(cfg)                                                             \
    __attribute__((constructor)) static void _register_layout() {                        \
        static _layout_context_t ctx = { &cfg, NULL };                                   \
        _layout_context_t **current = &_layout_head;                                     \
        while (true) {                                                                   \
            if (!*current || strcmp(ctx.config->title, (*current)->config->title) < 0) { \
                ctx.next = *current;                                                     \
                *current = &ctx;                                                         \
                return;                                                                  \
            }                                                                            \
            current = &(*current)->next;                                                 \
        }                                                                                \
    }

// util macros
#define ARRAY_SIZE(...) (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))
#define SIZED_ARRAY(...) { ARRAY_SIZE((__VA_ARGS__)), __VA_ARGS__ }

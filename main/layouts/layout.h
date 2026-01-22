/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "esp_lvgl_port.h"
#include <string.h>

typedef struct {
    uint32_t key;
    uint16_t x, y, width, height;
} layout_key_t;

typedef enum {
    LAYOUT_INPUT_TYPE_NONE,
    LAYOUT_INPUT_TYPE_TRACKPAD,
    LAYOUT_INPUT_TYPE_MAX,
} layout_input_type_t;

typedef struct {
    layout_input_type_t type;
    uint16_t x, y, width, height;
} layout_input_t;

typedef struct {
    struct {
        size_t count;
        const layout_key_t *data;
    } keys;
    struct {
        size_t count;
        const layout_input_t *data;
    } inputs;
} layout_def_t;

typedef struct {
    const char *name;
    void (*build)(lv_obj_t *screen);
} layout_config_t;

typedef struct _layout_context _layout_context_t;
struct _layout_context {
    const layout_config_t *config;
    _layout_context_t *next;
};
extern _layout_context_t *_layout_head;
#define LAYOUT_REGISTER(cfg)                                                           \
    __attribute__((constructor)) static void _register_layout() {                      \
        static _layout_context_t ctx = { &cfg, NULL };                                 \
        _layout_context_t **current = &_layout_head;                                   \
        while (true) {                                                                 \
            if (!*current || strcmp(ctx.config->name, (*current)->config->name) < 0) { \
                ctx.next = *current;                                                   \
                *current = &ctx;                                                       \
                return;                                                                \
            }                                                                          \
            current = &(*current)->next;                                               \
        }                                                                              \
    }

// util macros
#define ARRAY_SIZE(...) (sizeof(__VA_ARGS__) / sizeof((__VA_ARGS__)[0]))
#define SIZED_ARRAY(...) { ARRAY_SIZE((__VA_ARGS__)), __VA_ARGS__ }

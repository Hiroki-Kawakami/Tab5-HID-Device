/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "layout.h"
#include "hid_device.h"
#include "esp_log.h"

#define ROW1_HEIGHT 30

struct key {
    const char *label;
    uint8_t code;
    int16_t width;
};
struct row {
    int16_t height;
    struct { int size; const struct key *ptr; } keys;
};
struct keyboard {
    int size;
    struct row *rows;
};

// 画面幅640px基準、各行で1キーをflex(width=0)にして横幅を埋める
#define K  44  // 通常キー幅
#define T  56  // Tab幅
#define C  76  // Caps幅
#define E  80  // Enter幅
#define S  88  // Shift幅

static struct keyboard keyboard = SIZED_ARRAY((struct row[]){
    // Row 1: Function keys (14キー、Escをflexで埋める)
    {
        .height = 30,
        .keys = SIZED_ARRAY((struct key[]){
            { "Esc", 0x29 },
            { "F1", 0x3A }, { "F2", 0x3B }, { "F3", 0x3C }, { "F4", 0x3D },
            { "F5", 0x3E }, { "F6", 0x3F }, { "F7", 0x40 }, { "F8", 0x41 },
            { "F9", 0x42 }, { "F10", 0x43 }, { "F11", 0x44 }, { "F12", 0x45 },
            { "Del", 0x4C },
        }),
    },
    // Row 2: Number row (Backをflexで埋める)
    {
        .height = 40,
        .keys = SIZED_ARRAY((struct key[]){
            { "`", 0x35, K },
            { "1", 0x1E, K }, { "2", 0x1F, K }, { "3", 0x20, K }, { "4", 0x21, K }, { "5", 0x22, K },
            { "6", 0x23, K }, { "7", 0x24, K }, { "8", 0x25, K }, { "9", 0x26, K }, { "0", 0x27, K },
            { "-", 0x2D, K }, { "=", 0x2E, K },
            { "Back", 0x2A },  // flex
        }),
    },
    // Row 3: QWERTY row (\\をflexで埋める)
    {
        .height = 40,
        .keys = SIZED_ARRAY((struct key[]){
            { "Tab", 0x2B, T },
            { "Q", 0x14, K }, { "W", 0x1A, K }, { "E", 0x08, K }, { "R", 0x15, K }, { "T", 0x17, K },
            { "Y", 0x1C, K }, { "U", 0x18, K }, { "I", 0x0C, K }, { "O", 0x12, K }, { "P", 0x13, K },
            { "[", 0x2F, K }, { "]", 0x30, K },
            { "\\", 0x31 },  // flex
        }),
    },
    // Row 4: ASDF row (Enterをflexで埋める)
    {
        .height = 40,
        .keys = SIZED_ARRAY((struct key[]){
            { "Caps", 0x39, C },
            { "A", 0x04, K }, { "S", 0x16, K }, { "D", 0x07, K }, { "F", 0x09, K }, { "G", 0x0A, K },
            { "H", 0x0B, K }, { "J", 0x0D, K }, { "K", 0x0E, K }, { "L", 0x0F, K }, { ";", 0x33, K },
            { "'", 0x34, K },
            { "Enter", 0x28 },  // flex
        }),
    },
    // Row 5: ZXCV row (右Shiftをflexで埋める)
    {
        .height = 40,
        .keys = SIZED_ARRAY((struct key[]){
            { "Shift", 0xE1, S },
            { "Z", 0x1D, K }, { "X", 0x1B, K }, { "C", 0x06, K }, { "V", 0x19, K }, { "B", 0x05, K },
            { "N", 0x11, K }, { "M", 0x10, K }, { ",", 0x36, K }, { ".", 0x37, K }, { "/", 0x38, K },
            { "Shift", 0xE5 },  // flex
        }),
    },
    // Row 6: Bottom row (Spaceをflexで埋める)
    {
        .height = 40,
        .keys = SIZED_ARRAY((struct key[]){
            { "Ctrl", 0xE0, C },
            { "Win", 0xE3, K },
            { "Alt", 0xE2, K },
            { " ", 0x2C },  // flex
            { "Alt", 0xE6, K },
            { "Fn", 0x00, K },
            { "Ctrl", 0xE4, C },
        }),
    },
});

static void button_event(lv_event_t* event) {
    const struct key *key = lv_event_get_user_data(event);
    if (!key->code) return;

    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        // hid_device_key_press(key->code);
    } else if (code == LV_EVENT_RELEASED) {
        // hid_device_key_release(key->code);
    } else if (code == LV_EVENT_PRESSING) {
        // ボタン範囲外に出たらリリース
        lv_obj_t *btn = lv_event_get_target(event);
        lv_indev_t *indev = lv_indev_active();
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        if (!lv_obj_hit_test(btn, &point)) {
            // hid_device_key_release(key->code);
            lv_obj_remove_state(btn, LV_STATE_PRESSED);
        }
    }
}

static void build(lv_obj_t *screen) {
    int width = lv_obj_get_width(screen);

    lv_obj_t *prev_row_obj = NULL;
    for (int r = 0; r < keyboard.size; r++) {
        struct row *row = &keyboard.rows[r];
        lv_obj_t *row_obj = lv_obj_create(screen);
        lv_obj_set_size(row_obj, width, row->height);
        if (prev_row_obj) {
            lv_obj_align_to(row_obj, prev_row_obj, LV_ALIGN_OUT_BOTTOM_MID, 0, -1);
        } else {
            lv_obj_align(row_obj, LV_ALIGN_TOP_MID, 0, 0);
        }
        lv_obj_set_flex_flow(row_obj, LV_FLEX_FLOW_ROW);
        lv_obj_set_style_pad_column(row_obj, -1, 0);
        lv_obj_remove_flag(row_obj, LV_OBJ_FLAG_SCROLLABLE);

        for (int c = 0; c < row->keys.size; c++) {
            const struct key *key = &row->keys.ptr[c];
            lv_obj_t *button = lv_button_create(row_obj);
            lv_obj_set_height(button, row->height);
            if (key->width) {
                lv_obj_set_width(button, key->width);
            } else {
                lv_obj_set_flex_grow(button, 1);
            }
            lv_obj_add_event_cb(button, button_event, LV_EVENT_PRESSED, (void *)key);
            lv_obj_add_event_cb(button, button_event, LV_EVENT_RELEASED, (void *)key);
            lv_obj_add_event_cb(button, button_event, LV_EVENT_PRESSING, (void *)key);
            lv_obj_t *label = lv_label_create(button);
            lv_label_set_text(label, key->label);
            lv_obj_center(label);
        }

        prev_row_obj = row_obj;
    }
}

static const layout_config_t layout_config = {
    .name = "1. US",
    .build = build,
};
LAYOUT_REGISTER(layout_config)

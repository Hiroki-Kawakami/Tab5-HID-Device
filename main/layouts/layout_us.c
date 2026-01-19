/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "hid_device_mouse.h"
#include "layout.h"
#include "hid_device.h"
#include "hid_device_key.h"
#include "hid_device_keyboard.h"
#include "esp_log.h"

#define ROW1_HEIGHT 30

struct key {
    const char *label;
    uint32_t value;
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
            { "Esc", HID_DEVICE_KEY_ESCAPE },
            { "F1", HID_DEVICE_KEY_F1 },
            { "F2", HID_DEVICE_KEY_F2 },
            { "F3", HID_DEVICE_KEY_F3 },
            { "F4", HID_DEVICE_KEY_F4 },
            { "F5", HID_DEVICE_KEY_F5 },
            { "F6", HID_DEVICE_KEY_F6 },
            { "F7", HID_DEVICE_KEY_F7 },
            { "F8", HID_DEVICE_KEY_F8 },
            { "F9", HID_DEVICE_KEY_F9 },
            { "F10", HID_DEVICE_KEY_F10 },
            { "F11", HID_DEVICE_KEY_F11 },
            { "F12", HID_DEVICE_KEY_F12 },
            { "Del", HID_DEVICE_KEY_DELETE },
        }),
    },
    // Row 2: Number row (Backをflexで埋める)
    {
        .height = 40,
        .keys = SIZED_ARRAY((struct key[]){
            { "`", HID_DEVICE_KEY_GRAVE, K },
            { "1", HID_DEVICE_KEY_1, K },
            { "2", HID_DEVICE_KEY_2, K },
            { "3", HID_DEVICE_KEY_3, K },
            { "4", HID_DEVICE_KEY_4, K },
            { "5", HID_DEVICE_KEY_5, K },
            { "6", HID_DEVICE_KEY_6, K },
            { "7", HID_DEVICE_KEY_7, K },
            { "8", HID_DEVICE_KEY_8, K },
            { "9", HID_DEVICE_KEY_9, K },
            { "0", HID_DEVICE_KEY_0, K },
            { "-", HID_DEVICE_KEY_MINUS, K },
            { "=", HID_DEVICE_KEY_EQUAL, K },
            { "Back", HID_DEVICE_KEY_BACKSPACE },  // flex
        }),
    },
    // Row 3: QWERTY row (\\をflexで埋める)
    {
        .height = 40,
        .keys = SIZED_ARRAY((struct key[]){
            { "Tab", HID_DEVICE_KEY_TAB, T },
            { "Q", HID_DEVICE_KEY_Q, K },
            { "W", HID_DEVICE_KEY_W, K },
            { "E", HID_DEVICE_KEY_E, K },
            { "R", HID_DEVICE_KEY_R, K },
            { "T", HID_DEVICE_KEY_T, K },
            { "Y", HID_DEVICE_KEY_Y, K },
            { "U", HID_DEVICE_KEY_U, K },
            { "I", HID_DEVICE_KEY_I, K },
            { "O", HID_DEVICE_KEY_O, K },
            { "P", HID_DEVICE_KEY_P, K },
            { "[", HID_DEVICE_KEY_LEFT_BRACKET, K },
            { "]", HID_DEVICE_KEY_RIGHT_BRACKET, K },
            { "\\", HID_DEVICE_KEY_BACKSLASH },  // flex
        }),
    },
    // Row 4: ASDF row (Enterをflexで埋める)
    {
        .height = 40,
        .keys = SIZED_ARRAY((struct key[]){
            { "Caps", HID_DEVICE_KEY_CAPS_LOCK, C },
            { "A", HID_DEVICE_KEY_A, K },
            { "S", HID_DEVICE_KEY_S, K },
            { "D", HID_DEVICE_KEY_D, K },
            { "F", HID_DEVICE_KEY_F, K },
            { "G", HID_DEVICE_KEY_G, K },
            { "H", HID_DEVICE_KEY_H, K },
            { "J", HID_DEVICE_KEY_J, K },
            { "K", HID_DEVICE_KEY_K, K },
            { "L", HID_DEVICE_KEY_L, K },
            { ";", HID_DEVICE_KEY_SEMICOLON, K },
            { "'", HID_DEVICE_KEY_QUOTE, K },
            { "Enter", HID_DEVICE_KEY_ENTER },  // flex
        }),
    },
    // Row 5: ZXCV row (右Shiftをflexで埋める)
    {
        .height = 40,
        .keys = SIZED_ARRAY((struct key[]){
            { "Shift", HID_DEVICE_KEY_LEFT_SHIFT, S },
            { "Z", HID_DEVICE_KEY_Z, K },
            { "X", HID_DEVICE_KEY_X, K },
            { "C", HID_DEVICE_KEY_C, K },
            { "V", HID_DEVICE_KEY_V, K },
            { "B", HID_DEVICE_KEY_B, K },
            { "N", HID_DEVICE_KEY_N, K },
            { "M", HID_DEVICE_KEY_M, K },
            { ",", HID_DEVICE_KEY_COMMA, K },
            { ".", HID_DEVICE_KEY_DOT, K },
            { "/", HID_DEVICE_KEY_SLASH, K },
            { "Shift", HID_DEVICE_KEY_RIGHT_SHIFT },  // flex
        }),
    },
    // Row 6: Bottom row (Spaceをflexで埋める)
    {
        .height = 40,
        .keys = SIZED_ARRAY((struct key[]){
            { "Ctrl", HID_DEVICE_KEY_LEFT_CTRL, C },
            { "Win", HID_DEVICE_KEY_LEFT_GUI, K },
            { "Alt", HID_DEVICE_KEY_LEFT_ALT, K },
            { " ", HID_DEVICE_KEY_SPACE },  // flex
            { "Alt", HID_DEVICE_KEY_RIGHT_ALT, K },
            { "Win", HID_DEVICE_KEY_RIGHT_GUI, K },
            { "Fn", HID_DEVICE_KEY_NONE, K },
            { "Ctrl", HID_DEVICE_KEY_RIGHT_CTRL, C },
        }),
    },
});

#define TRACKPAD_CLICK_TIMEOUT_MS 200

static lv_point_t trackpad_last_point;
static bool trackpad_moved;
static uint32_t trackpad_press_time;

static void trackpad_event(lv_event_t *event) {
    lv_event_code_t code = lv_event_get_code(event);
    lv_indev_t *indev = lv_indev_active();
    lv_point_t point;
    lv_indev_get_point(indev, &point);

    if (code == LV_EVENT_PRESSED) {
        trackpad_last_point = point;
        trackpad_moved = false;
        trackpad_press_time = lv_tick_get();
    } else if (code == LV_EVENT_PRESSING) {
        int dx = point.x - trackpad_last_point.x;
        int dy = point.y - trackpad_last_point.y;
        trackpad_last_point = point;
        if (dx != 0 || dy != 0) {
            trackpad_moved = true;
            hid_device_mouse_move(dx * 2, dy * 2);
        }
    } else if (code == LV_EVENT_RELEASED) {
        uint32_t elapsed = lv_tick_elaps(trackpad_press_time);
        if (!trackpad_moved && elapsed < TRACKPAD_CLICK_TIMEOUT_MS) {
            hid_device_mouse_click(HID_DEVICE_MOUSE_BUTTON_LEFT);
        }
    }
}

static void mouse_button_event(lv_event_t *event) {
    hid_device_mouse_button_t button = (hid_device_mouse_button_t)(intptr_t)lv_event_get_user_data(event);
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        hid_device_mouse_press_button(button);
    } else if (code == LV_EVENT_RELEASED) {
        hid_device_mouse_release_button(button);
    }
}

static void button_event(lv_event_t* event) {
    const struct key *key = lv_event_get_user_data(event);
    if (!key->value) return;

    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        hid_device_keyboard_press_key(key->value);
    } else if (code == LV_EVENT_RELEASED) {
        hid_device_keyboard_release_key(key->value);
    } else if (code == LV_EVENT_PRESSING) {
        // ボタン範囲外に出たらリリース
        lv_obj_t *btn = lv_event_get_target(event);
        lv_indev_t *indev = lv_indev_active();
        lv_point_t point;
        lv_indev_get_point(indev, &point);
        if (!lv_obj_hit_test(btn, &point)) {
            hid_device_keyboard_release_key(key->value);
            lv_obj_remove_state(btn, LV_STATE_PRESSED);
        }
    }
}

static void build(lv_obj_t *screen) {
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x000000), LV_PART_MAIN);
    int width = lv_obj_get_width(screen), height = lv_obj_get_height(screen);

    lv_obj_t *prev_row_obj = NULL;
    int keyboard_height = 1;
    for (int r = 0; r < keyboard.size; r++) {
        struct row *row = &keyboard.rows[r];
        lv_obj_t *row_obj = lv_obj_create(screen);
        lv_obj_remove_style_all(row_obj);
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
            lv_obj_remove_style_all(button);
            lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
            lv_obj_set_style_border_color(button, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_set_style_bg_color(button, lv_color_hex(0x000000), LV_PART_MAIN);
            lv_obj_set_style_bg_color(button, lv_color_hex(0x333333), LV_STATE_PRESSED);
            lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
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
            lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
            lv_obj_center(label);
        }

        prev_row_obj = row_obj;
        keyboard_height += row->height - 1;
    }

    lv_obj_t *trackpad = lv_obj_create(screen);
    lv_obj_remove_style_all(trackpad);
    lv_obj_set_size(trackpad, width / 5 * 2, height - keyboard_height);
    lv_obj_align(trackpad, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(trackpad, lv_color_hex(0x1A1A1A), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(trackpad, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_event_cb(trackpad, trackpad_event, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(trackpad, trackpad_event, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(trackpad, trackpad_event, LV_EVENT_RELEASED, NULL);

    lv_obj_t *mouse_buttons = lv_obj_create(screen);
    lv_obj_remove_style_all(mouse_buttons);
    lv_obj_set_size(mouse_buttons, width / 10 * 3 - 20, 50);
    lv_obj_align(mouse_buttons, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_set_style_border_width(mouse_buttons, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(mouse_buttons, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    lv_obj_t *mouse_left_btn = lv_button_create(mouse_buttons);
    lv_obj_remove_style_all(mouse_left_btn);
    lv_obj_set_size(mouse_left_btn, lv_pct(50), lv_pct(100));
    lv_obj_align(mouse_left_btn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_color(mouse_left_btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(mouse_left_btn, lv_color_hex(0x333333), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(mouse_left_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_event_cb(mouse_left_btn, mouse_button_event, LV_EVENT_PRESSED, (void *)HID_DEVICE_MOUSE_BUTTON_LEFT);
    lv_obj_add_event_cb(mouse_left_btn, mouse_button_event, LV_EVENT_RELEASED, (void *)HID_DEVICE_MOUSE_BUTTON_LEFT);

    lv_obj_t *mouse_right_btn = lv_button_create(mouse_buttons);
    lv_obj_remove_style_all(mouse_right_btn);
    lv_obj_set_size(mouse_right_btn, lv_pct(50), lv_pct(100));
    lv_obj_align(mouse_right_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_color(mouse_right_btn, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_style_bg_color(mouse_right_btn, lv_color_hex(0x333333), LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(mouse_right_btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_event_cb(mouse_right_btn, mouse_button_event, LV_EVENT_PRESSED, (void *)HID_DEVICE_MOUSE_BUTTON_RIGHT);
    lv_obj_add_event_cb(mouse_right_btn, mouse_button_event, LV_EVENT_RELEASED, (void *)HID_DEVICE_MOUSE_BUTTON_RIGHT);

    lv_obj_t *mouse_separator = lv_obj_create(mouse_buttons);
    lv_obj_remove_style_all(mouse_separator);
    lv_obj_set_size(mouse_separator, 1, 40);
    lv_obj_center(mouse_separator);
    lv_obj_set_style_bg_color(mouse_separator, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(mouse_separator, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *arrow_buttons = lv_obj_create(screen);
    lv_obj_remove_style_all(arrow_buttons);
    lv_obj_set_size(arrow_buttons, width / 10 * 3 - 20, 80);
    lv_obj_align(arrow_buttons, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    // 矢印キーの定義
    static const struct {
        struct key key;
        lv_border_side_t border;
        lv_align_t align;
    } arrow_keys[] = {
        {{ LV_SYMBOL_UP   , HID_DEVICE_KEY_UP    }, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT  | LV_BORDER_SIDE_RIGHT , LV_ALIGN_TOP_MID      },
        {{ LV_SYMBOL_DOWN , HID_DEVICE_KEY_DOWN  }, LV_BORDER_SIDE_BOTTOM                                            , LV_ALIGN_BOTTOM_MID   },
        {{ LV_SYMBOL_LEFT , HID_DEVICE_KEY_LEFT  }, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT  | LV_BORDER_SIDE_BOTTOM, LV_ALIGN_BOTTOM_LEFT  },
        {{ LV_SYMBOL_RIGHT, HID_DEVICE_KEY_RIGHT }, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_RIGHT | LV_BORDER_SIDE_BOTTOM, LV_ALIGN_BOTTOM_RIGHT },
    };
    for (int i = 0; i < (sizeof(arrow_keys) / sizeof(arrow_keys[0])); i++) {
        lv_obj_t *button = lv_button_create(arrow_buttons);
        lv_obj_remove_style_all(button);
        lv_obj_set_size(button, lv_pct(34), lv_pct(50));
        lv_obj_align(button, arrow_keys[i].align, 0, 0);
        lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(button, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_set_style_border_side(button, arrow_keys[i].border, LV_PART_MAIN);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_obj_set_style_bg_color(button, lv_color_hex(0x333333), LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_add_event_cb(button, button_event, LV_EVENT_PRESSED, (void *)&arrow_keys[i].key);
        lv_obj_add_event_cb(button, button_event, LV_EVENT_RELEASED, (void *)&arrow_keys[i].key);
        lv_obj_add_event_cb(button, button_event, LV_EVENT_PRESSING, (void *)&arrow_keys[i].key);
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, arrow_keys[i].key.label);
        lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN);
        lv_obj_center(label);
    }
}

static const layout_config_t layout_config = {
    .name = "1. US",
    .build = build,
};
LAYOUT_REGISTER(layout_config)

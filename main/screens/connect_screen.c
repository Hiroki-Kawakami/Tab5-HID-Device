/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "connect_screen.h"
#include "hid_device.h"

typedef struct {
    connect_screen_config_t config;
    lv_obj_t *screen;
    lv_obj_t *title_label;
} connect_screen_t;

static void create_navigation_bar(connect_screen_t *screen) {
    lv_obj_t *navigation_bar = lv_obj_create(screen->screen);
    lv_obj_set_size(navigation_bar, LV_PCT(100), 60);
    lv_obj_align(navigation_bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_border_width(navigation_bar, 1, 0);
    lv_obj_set_style_border_side(navigation_bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(navigation_bar, lv_color_hex(0x808080), 0);

    screen->title_label = lv_label_create(navigation_bar);
    const char *title = screen->config.mode == CONNECT_SCREEN_MODE_PAIRING ? "Device Pairing" : "Device Connect";
    lv_label_set_text(screen->title_label, title);
    lv_obj_align(screen->title_label, LV_ALIGN_LEFT_MID, 10, 0);
}

static void cancel_btn_clicked(lv_event_t *e) {
    hid_device_stop_pairing();
}

static void add_new_device_btn_clicked(lv_event_t *e) {
    hid_device_start_pairing();
}

static void create_connect_indicator(connect_screen_t *screen) {
    lv_obj_t *container = lv_obj_create(screen->screen);
    lv_obj_set_size(container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_center(container);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(container, 20, 0);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);

    lv_obj_t *spinner = lv_spinner_create(container);
    lv_obj_set_size(spinner, 50, 50);

    lv_obj_t *message_label = lv_label_create(container);
    if (screen->config.mode == CONNECT_SCREEN_MODE_PAIRING) {
        lv_label_set_text(message_label, "Waiting for pairing...");
    } else {
        lv_label_set_text_fmt(message_label, "Connecting to %s...", screen->config.device_name);
    }
    lv_obj_set_style_pad_top(message_label, 15, 0);

    if (screen->config.mode == CONNECT_SCREEN_MODE_PAIRING) {
        if (screen->config.cancellable) {
            lv_obj_t *cancel_btn = lv_btn_create(container);
            lv_obj_set_style_pad_top(cancel_btn, 15, 0);
            lv_obj_add_event_cb(cancel_btn, cancel_btn_clicked, LV_EVENT_CLICKED, NULL);

            lv_obj_t *cancel_label = lv_label_create(cancel_btn);
            lv_label_set_text(cancel_label, "Cancel");
            lv_obj_center(cancel_label);
        }
    } else {
        lv_obj_t *add_btn = lv_btn_create(container);
        lv_obj_set_style_pad_top(add_btn, 15, 0);
        lv_obj_add_event_cb(add_btn, add_new_device_btn_clicked, LV_EVENT_CLICKED, NULL);

        lv_obj_t *add_label = lv_label_create(add_btn);
        lv_label_set_text(add_label, "Add New Device");
        lv_obj_center(add_label);
    }
}

static void screen_delete_cb(lv_event_t *e) {
    connect_screen_t *connect_screen = lv_event_get_user_data(e);
    if (connect_screen) {
        lv_free(connect_screen);
    }
}

void connect_screen_open(lv_obj_t *screen, connect_screen_config_t *config) {
    connect_screen_t *connect_screen = (connect_screen_t*)lv_malloc(sizeof(connect_screen_t));
    lv_obj_set_user_data(screen, connect_screen);
    lv_obj_add_event_cb(screen, screen_delete_cb, LV_EVENT_DELETE, connect_screen);

    connect_screen->config = *config;
    connect_screen->screen = screen;
    create_navigation_bar(connect_screen);
    create_connect_indicator(connect_screen);
}

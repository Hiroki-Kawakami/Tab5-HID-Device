/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "connect_screen.h"
#include "hid_device.h"
#include "misc/lv_async.h"
#include "stdlib/lv_mem.h"
#include <stdint.h>
#include <inttypes.h>
#include <string.h>

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

typedef struct passkey_info passkey_info_t;
struct passkey_info {
    connect_screen_t *screen;
    uint32_t passkey;
    void (*show)(passkey_info_t *info);
};

static void confirm_btn_clicked(lv_event_t *e) {
    lv_obj_t *msgbox = lv_event_get_user_data(e);
    hid_device_passkey_confirm(true);
    lv_msgbox_close(msgbox);
}

static void cancel_numcmp_btn_clicked(lv_event_t *e) {
    lv_obj_t *msgbox = lv_event_get_user_data(e);
    hid_device_passkey_confirm(false);
    lv_msgbox_close(msgbox);
}

static void show_numeric_comparison(passkey_info_t *info) {
    char passkey_str[64];
    snprintf(passkey_str, sizeof(passkey_str), "Confirm passkey:\n\n%06" PRIu32, info->passkey);

    lv_obj_t *msgbox = lv_msgbox_create(info->screen->screen);
    lv_msgbox_add_title(msgbox, "Numeric Comparison");
    lv_msgbox_add_text(msgbox, passkey_str);

    lv_obj_t *confirm_btn = lv_msgbox_add_footer_button(msgbox, "Confirm");
    lv_obj_add_event_cb(confirm_btn, confirm_btn_clicked, LV_EVENT_CLICKED, msgbox);

    lv_obj_t *cancel_btn = lv_msgbox_add_footer_button(msgbox, "Cancel");
    lv_obj_add_event_cb(cancel_btn, cancel_numcmp_btn_clicked, LV_EVENT_CLICKED, msgbox);

    lv_obj_center(msgbox);
}

static void passkey_display_close_btn_clicked(lv_event_t *e) {
    lv_obj_t *msgbox = lv_event_get_user_data(e);
    lv_msgbox_close(msgbox);
}

static void show_passkey_display(passkey_info_t *info) {
    char passkey_str[64];
    snprintf(passkey_str, sizeof(passkey_str), "Enter this passkey on the host device:\n\n%06" PRIu32, info->passkey);

    lv_obj_t *msgbox = lv_msgbox_create(info->screen->screen);
    lv_msgbox_add_title(msgbox, "Passkey");
    lv_msgbox_add_text(msgbox, passkey_str);

    lv_obj_t *close_btn = lv_msgbox_add_footer_button(msgbox, "OK");
    lv_obj_add_event_cb(close_btn, passkey_display_close_btn_clicked, LV_EVENT_CLICKED, msgbox);

    lv_obj_center(msgbox);
}

static void show_passkey_info_async(void *user_data) {
    passkey_info_t *info = user_data;
    info->show(info);
    lv_free(info);
}

static void hid_device_notify_callback(hid_device_notify_t *notify, void *user_data) {
    if (notify->type == HID_DEVICE_NOTIFY_PASSKEY_CONFIRM) {
        passkey_info_t *info = lv_malloc(sizeof(passkey_info_t));
        info->screen = user_data;
        info->passkey = notify->passkey.passkey;
        info->show = show_numeric_comparison;
        lv_async_call(show_passkey_info_async, (void*)info);
    } else if (notify->type == HID_DEVICE_NOTIFY_PASSKEY_DISPLAY) {
        passkey_info_t *info = lv_malloc(sizeof(passkey_info_t));
        info->screen = user_data;
        info->passkey = notify->passkey.passkey;
        info->show = show_passkey_display;
        lv_async_call(show_passkey_info_async, (void*)info);
    }
}

static void screen_delete_cb(lv_event_t *e) {
    connect_screen_t *connect_screen = lv_event_get_user_data(e);
    if (connect_screen) {
        lv_free(connect_screen);
        hid_device_remove_notify_callback(hid_device_notify_callback, connect_screen);
    }
}

void connect_screen_open(lv_obj_t *screen, connect_screen_config_t *config) {
    connect_screen_t *connect_screen = (connect_screen_t*)lv_malloc(sizeof(connect_screen_t));
    hid_device_add_notify_callback(hid_device_notify_callback, connect_screen);
    lv_obj_set_user_data(screen, connect_screen);
    lv_obj_add_event_cb(screen, screen_delete_cb, LV_EVENT_DELETE, connect_screen);

    connect_screen->config = *config;
    connect_screen->screen = screen;
    create_navigation_bar(connect_screen);
    create_connect_indicator(connect_screen);
}

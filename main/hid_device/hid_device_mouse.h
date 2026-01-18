/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    HID_DEVICE_MOUSE_BUTTON_LEFT,
    HID_DEVICE_MOUSE_BUTTON_RIGHT,
} hid_device_mouse_button_t;

void hid_device_mouse_init(void);
void hid_device_mouse_move(int8_t dx, int8_t dy);
void hid_device_mouse_click(hid_device_mouse_button_t button);
void hid_device_mouse_press_button(hid_device_mouse_button_t button);
void hid_device_mouse_release_button(hid_device_mouse_button_t button);

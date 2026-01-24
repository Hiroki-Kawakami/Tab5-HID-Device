/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "hid_device_mouse.h"
#include "hid_device.h"

#define MOUSE_REPORT_ID 2

static uint8_t pressed_buttons = 0;

static uint8_t button_mask(hid_device_mouse_button_t button) {
    return (button == HID_DEVICE_MOUSE_BUTTON_LEFT) ? 0x01 : 0x02;
}

static void send_report(uint8_t buttons, int8_t dx, int8_t dy, int8_t wheel) {
    uint8_t *report = malloc(4);
    report[0] = buttons;
    report[1] = dx;
    report[2] = dy;
    report[3] = wheel;
    hid_device_send_report(MOUSE_REPORT_ID, report, sizeof(report), true);
}

void hid_device_mouse_init(void) {
    pressed_buttons = 0;
}

void hid_device_mouse_move(int8_t dx, int8_t dy) {
    send_report(pressed_buttons, dx, dy, 0);
}

void hid_device_mouse_click(hid_device_mouse_button_t button) {
    uint8_t mask = button_mask(button);
    if (pressed_buttons & mask) {
        return;  // Button already pressed, ignore click
    }

    send_report(pressed_buttons | mask, 0, 0, 0);  // Press
    send_report(pressed_buttons, 0, 0, 0);          // Release
}

void hid_device_mouse_press_button(hid_device_mouse_button_t button) {
    uint8_t mask = button_mask(button);
    if (pressed_buttons & mask) {
        return;  // Already pressed
    }
    pressed_buttons |= mask;
    send_report(pressed_buttons, 0, 0, 0);
}

void hid_device_mouse_release_button(hid_device_mouse_button_t button) {
    uint8_t mask = button_mask(button);
    if (!(pressed_buttons & mask)) {
        return;  // Not pressed
    }
    pressed_buttons &= ~mask;
    send_report(pressed_buttons, 0, 0, 0);
}

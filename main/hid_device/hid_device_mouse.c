/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "hid_device_mouse.h"
#include "hid_device/hid_device.h"

#define MOUSE_REPORT_ID 2

void hid_device_mouse_init(void) {
    // Nothing to initialize for now
}

void hid_device_mouse_move(int8_t dx, int8_t dy) {
    uint8_t report[4] = {0, dx, dy, 0};
    hid_device_send_report(MOUSE_REPORT_ID, report, sizeof(report), false);
}

void hid_device_mouse_click(hid_device_mouse_button_t button) {
    uint8_t button_mask = (button == HID_DEVICE_MOUSE_BUTTON_LEFT) ? 0x01 : 0x02;

    // Press
    uint8_t press_report[4] = {button_mask, 0, 0, 0};
    hid_device_send_report(MOUSE_REPORT_ID, press_report, sizeof(press_report), false);

    // Release
    uint8_t release_report[4] = {0, 0, 0, 0};
    hid_device_send_report(MOUSE_REPORT_ID, release_report, sizeof(release_report), false);
}

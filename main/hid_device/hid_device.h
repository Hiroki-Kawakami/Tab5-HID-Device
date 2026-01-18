/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HID_DEVICE_APPEARANCE_GENERIC,
    HID_DEVICE_APPEARANCE_KEYBOARD,
    HID_DEVICE_APPEARANCE_MOUSE,
    HID_DEVICE_APPEARANCE_JOYSTICK,
    HID_DEVICE_APPEARANCE_GAMEPAD,
} hid_device_appearance_t;

typedef struct {
    uint16_t vendor_id, product_id, version;
    const char *device_name, *manufacturer_name, *serial_number;
    hid_device_appearance_t appearance;
    struct {
        const uint8_t *data;
        size_t size;
    } report_map;
} hid_device_profile_t;

typedef enum {
    HID_DEVICE_STATE_BEGIN,
    HID_DEVICE_STATE_WAIT_CONNECT,
    HID_DEVICE_STATE_PAIRING,
    HID_DEVICE_STATE_ACTIVE,
    HID_DEVICE_STATE_INACTIVE,
    HID_DEVICE_STATE_MAX,
} hid_device_state_t;

typedef struct {
    enum {
        HID_DEVICE_NOTIFY_STATE_CHANGED,
        HID_DEVICE_NOTIFY_PASSKEY_DISPLAY,
        HID_DEVICE_NOTIFY_PASSKEY_INPUT,
        HID_DEVICE_NOTIFY_PASSKEY_CONFIRM,
    } type;
    union {
        struct {
            hid_device_state_t prev;
            hid_device_state_t current;
        } state;
        struct {
            uint32_t passkey;
        } passkey;
    };
} hid_device_notify_t;
typedef void (*hid_device_notify_callback_t)(hid_device_notify_t *notify, void *user_data);

esp_err_t hid_device_init(const hid_device_profile_t *profile);
void hid_device_add_notify_callback(hid_device_notify_callback_t callback, void *user_data);
void hid_device_remove_notify_callback(hid_device_notify_callback_t callback, void *user_data);
hid_device_state_t hid_device_state(void);
bool hid_device_is_connected(void);
void hid_device_start_pairing(void);
void hid_device_stop_pairing(void);
void hid_device_passkey_input(uint32_t passkey);
void hid_device_passkey_confirm(bool accept);
void hid_device_send_report(size_t report_id, uint8_t *report, size_t length);

// MARK: Profiles
extern const hid_device_profile_t hid_device_profile_keyboard;

#ifdef __cplusplus
}
#endif

/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "hid_device_keyboard.h"
#include "hid_device.h"
#include "hid_device_key.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define KEY_NUM_MAX 32
static SemaphoreHandle_t keys_mutex;
static uint32_t keys_buffer[2][KEY_NUM_MAX];
static uint32_t *current_pressed_keys = keys_buffer[0];

static void hid_device_keyboard_send(uint32_t *keys) {
    if (memcmp(current_pressed_keys, keys, sizeof(uint32_t) * KEY_NUM_MAX) == 0) return;
    uint8_t *report = calloc(1, 8);
    int key_index = 2;

    for (int i = 0; keys[i] && i < KEY_NUM_MAX; i++) {
        uint16_t code = HID_DEVICE_KEY_CODE(keys[i]);
        if (code >= 0xE0 && code <= 0xE7) {
            report[0] |= (1 << (code - 0xE0));
        } else if (key_index < 8) {
            report[key_index++] = code;
        }
    }

    hid_device_send_report(1, report, 8, true);
    current_pressed_keys = keys;
}

void hid_device_keyboard_press_keys(uint32_t *keys, size_t length) {
    xSemaphoreTake(keys_mutex, portMAX_DELAY);
    uint32_t *next_buffer = current_pressed_keys == keys_buffer[0] ? keys_buffer[1] : keys_buffer[0];
    int i = 0;
    for (; current_pressed_keys[i] && i < KEY_NUM_MAX; i++) {
        next_buffer[i] = current_pressed_keys[i];
    }
    for (int j = 0; i < KEY_NUM_MAX && j < length; j++) {
        bool includes = false;
        for (int k = 0; k < i; k++) {
            if (next_buffer[k] == keys[j]) {
                includes = true;
                break;
            }
        }
        if (!includes) next_buffer[i++] = keys[j];
    }
    if (i < KEY_NUM_MAX) next_buffer[i] = 0;
    hid_device_keyboard_send(next_buffer);
    xSemaphoreGive(keys_mutex);
}

void hid_device_keyboard_release_keys(uint32_t *keys, size_t length) {
    xSemaphoreTake(keys_mutex, portMAX_DELAY);
    uint32_t *next_buffer = current_pressed_keys == keys_buffer[0] ? keys_buffer[1] : keys_buffer[0];
    int i = 0;
    for (int j = 0; current_pressed_keys[j] && j < KEY_NUM_MAX; j++) {
        bool should_release = false;
        for (int k = 0; k < length; k++) {
            if (current_pressed_keys[j] == keys[k]) {
                should_release = true;
                break;
            }
        }
        if (!should_release) {
            next_buffer[i++] = current_pressed_keys[j];
        }
    }
    if (i < KEY_NUM_MAX) next_buffer[i] = 0;
    hid_device_keyboard_send(next_buffer);
    xSemaphoreGive(keys_mutex);
}

void hid_device_keyboard_press_key(uint32_t key) {
    hid_device_keyboard_press_keys(&key, 1);
}

void hid_device_keyboard_release_key(uint32_t key) {
    hid_device_keyboard_release_keys(&key, 1);
}

void hid_device_keyboard_init(void) {
    keys_mutex = xSemaphoreCreateMutex();
    assert(keys_mutex);
}

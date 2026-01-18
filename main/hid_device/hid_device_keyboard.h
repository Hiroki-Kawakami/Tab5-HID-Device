/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

void hid_device_keyboard_init(void);
void hid_device_keyboard_press_keys(uint32_t *keys, size_t length);
void hid_device_keyboard_release_keys(uint32_t *keys, size_t length);
void hid_device_keyboard_press_key(uint32_t key);
void hid_device_keyboard_release_key(uint32_t key);

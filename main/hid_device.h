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

/**
 * @brief Initialize the HID keyboard device
 *
 * This initializes the BLE HID device and starts advertising.
 * Must be called after BSP Bluetooth is enabled.
 *
 * @return ESP_OK on success
 */
esp_err_t hid_device_init(void);

/**
 * @brief Check if the HID device is connected to a host
 *
 * @return true if connected, false otherwise
 */
bool hid_device_connected(void);

/**
 * @brief Send a key press with optional modifier
 *
 * @param modifier Modifier keys (use HID_KEY_MOD_* constants)
 * @param keycode HID keycode to send
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not connected
 */
esp_err_t hid_device_send_key(uint8_t modifier, uint8_t keycode);

/**
 * @brief Release all keys
 *
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if not connected
 */
esp_err_t hid_device_release_keys(void);

/**
 * @brief Send a single character (press and release)
 *
 * Supports: a-z, A-Z, 0-9, space, newline
 *
 * @param c Character to send
 * @return ESP_OK on success, ESP_ERR_NOT_SUPPORTED if char not supported
 */
esp_err_t hid_device_send_char(char c);

/**
 * @brief Send a string of characters
 *
 * @param str Null-terminated string to send
 * @return ESP_OK on success
 */
esp_err_t hid_device_send_string(const char *str);

#ifdef __cplusplus
}
#endif

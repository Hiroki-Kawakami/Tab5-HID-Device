/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "bsp_private.h"
#include "bsp_tab5.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "misc/bsp_display.h"
#include "pi4io/pi4io.h"
#include "ili9881c/ili9881c.h"
#include "gt911/gt911.h"
#include "st7123/st7123_lcd.h"
#include "st7123/st7123_touch.h"
#include "nvs_flash.h"
#include "esp_hosted.h"
#ifdef CONFIG_BT_BLUEDROID_ENABLED
#include "esp_hosted_bt.h"
#endif
#ifdef CONFIG_BT_NIMBLE_ENABLED
#include "nimble/nimble_port.h"
#endif

static const char *TAG = "BSP_TAB5";

#define I2C0_PORT_NUM (0)
static i2c_master_bus_handle_t i2c0;
static pi4io_t pi4ioe1, pi4ioe2;

static void **frame_buffers;
static ili9881c_lcd_t ili9881c;
static gt911_touch_t gt911;
static st7123_lcd_t st7123_lcd;
static st7123_touch_t st7123_touch;

esp_err_t bsp_tab5_init(const bsp_tab5_config_t *config) {
    esp_err_t err;

    // Check config values
    bsp_tab5_config_t tmp_config = *config;
    if (!tmp_config.display.fb_num) tmp_config.display.fb_num = 1;
    config = &tmp_config;

    // Initialize I2C0 bus
    err = i2c_new_master_bus(&(i2c_master_bus_config_t){
        .i2c_port = I2C0_PORT_NUM,
        .sda_io_num = GPIO_NUM_31,
        .scl_io_num = GPIO_NUM_32,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .flags.enable_internal_pullup = true,
    }, &i2c0);
    BSP_RETURN_ERR(err);

    // Initialize PI4IOE1 (address 0x43)
    err = pi4io_init(i2c0, 0x43, (pi4io_pin_config_t[8]){
        [0] = { PI4IO_PIN_MODE_OUTPUT, .initial_value = false },  // RF_INT_EXT_SWITCH
        [1] = { PI4IO_PIN_MODE_OUTPUT, .initial_value = true },   // SPK_EN
        [2] = { PI4IO_PIN_MODE_OUTPUT, .initial_value = true },   // EXT5V_EN
        [4] = { PI4IO_PIN_MODE_OUTPUT, .initial_value = true },   // LCD_RST
        [5] = { PI4IO_PIN_MODE_OUTPUT, .initial_value = true },   // TP_RST
        [6] = { PI4IO_PIN_MODE_OUTPUT, .initial_value = true },   // CAM_RST
        [7] = { PI4IO_PIN_MODE_INPUT },                           // HP_DET
    }, &pi4ioe1);
    BSP_RETURN_ERR(err);

    // Initialize PI4IOE2 (address 0x44)
    err = pi4io_init(i2c0, 0x44, (pi4io_pin_config_t[8]){
        [0] = { PI4IO_PIN_MODE_OUTPUT, .initial_value = true },   // WLAN_PWR_EN
        [3] = { PI4IO_PIN_MODE_OUTPUT, .initial_value = false },  // USB5V_EN
        [4] = { PI4IO_PIN_MODE_OUTPUT, .initial_value = false },  // PWROFF_PLUSE
        [5] = { PI4IO_PIN_MODE_OUTPUT, .initial_value = false },  // nCHG_QC_EN
        [6] = { PI4IO_PIN_MODE_INPUT },                           // CHG_STAT
        [7] = { PI4IO_PIN_MODE_OUTPUT, .initial_value = false },  // CHG_EN
    }, &pi4ioe2);
    BSP_RETURN_ERR(err);

    // Reset Touch Panel and LCD
    gpio_reset_pin(GPIO_NUM_23);
    pi4io_set_output(pi4ioe1, 4, false);  // LCD_RST = Low
    pi4io_set_output(pi4ioe1, 5, false);  // TP_RST = Low
    vTaskDelay(pdMS_TO_TICKS(100));
    pi4io_set_output(pi4ioe1, 4, true);   // LCD_RST = High
    pi4io_set_output(pi4ioe1, 5, true);   // TP_RST = High
    vTaskDelay(pdMS_TO_TICKS(100));

    if (i2c_master_probe(i2c0, 0x55, 10) == ESP_OK) {
        // Initialize ST7123 LCD
        err = st7123_lcd_init(&(st7123_lcd_config_t){
            .backlight_gpio = GPIO_NUM_22,
            .size = (bsp_size_t){ 720, 1280 },
            .pixel_format = BSP_PIXEL_FORMAT_RGB565,
            .fb_num = config->display.fb_num,
        }, &st7123_lcd);
        BSP_RETURN_ERR(err);
        frame_buffers = st7123_lcd_get_frame_buffers(st7123_lcd);

        // Initialize ST7123 Touch Panel
        err = st7123_touch_init(&(st7123_touch_config_t){
            .i2c_bus = i2c0,
            .size = (bsp_size_t){ 720, 1280 },
            .int_gpio = GPIO_NUM_23,
            .rst_gpio = GPIO_NUM_NC,
            .scl_speed_hz = 100000,
        }, &st7123_touch);
        BSP_RETURN_ERR(err);
    } else if (i2c_master_probe(i2c0, 0x14, 10) == ESP_OK) {
        // Initialize ILI9881C LCD
        err = ili9881c_lcd_init(&(ili9881c_lcd_config_t){
            .backlight_gpio = GPIO_NUM_22,
            .size = (bsp_size_t){ 720, 1280 },
            .pixel_format = BSP_PIXEL_FORMAT_RGB565,
            .fb_num = config->display.fb_num,
        }, &ili9881c);
        BSP_RETURN_ERR(err);
        frame_buffers = ili9881c_lcd_get_frame_buffers(ili9881c);

        // Initialize GT911 Touch Panel
        err = gt911_touch_init(&(gt911_touch_config_t){
            .i2c_bus = i2c0,
            .size = (bsp_size_t){ 720, 1280 },
            .int_gpio = GPIO_NUM_23,
            .rst_gpio = GPIO_NUM_NC,
            .scl_speed_hz = 100000,
            .interrupt = config->touch.interrupt,
        }, &gt911);
        BSP_RETURN_ERR(err);
    } else {
        return ESP_ERR_NOT_FOUND;
    }

    if (config->wifi.enable || config->bluetooth.enable) {
        // NVS (for WiFi & Bluetooth)
        err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            if ((err = nvs_flash_erase()) == ESP_OK) {
                err = nvs_flash_init();
            }
        }
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize NVS flash");
            return err;
        }
    }

    // WiFi
    if (config->wifi.enable) {
        ESP_LOGE(TAG, "WiFi initialization not implemented yet!");
        assert(0);
        // ESP_ERROR_CHECK(esp_netif_init());
        // ESP_ERROR_CHECK(esp_event_loop_create_default());
        // esp_netif_create_default_wifi_ap();
        // wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        // ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    }

    // Bluetooth
    if (config->bluetooth.enable) {
#if defined(CONFIG_BT_BLUEDROID_ENABLED)
        /* initialize TRANSPORT first */
        hosted_hci_bluedroid_open();

        /* get HCI driver operations */
        esp_bluedroid_hci_driver_operations_t operations = {
            .send = hosted_hci_bluedroid_send,
            .check_send_available = hosted_hci_bluedroid_check_send_available,
            .register_host_callback = hosted_hci_bluedroid_register_host_callback,
        };
        esp_bluedroid_attach_hci_driver(&operations);
#elif defined(CONFIG_BT_NIMBLE_ENABLED)
        err = nimble_port_init();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize NimBLE");
            return err;
        }
#else
        ESP_LOGE(TAG, "Bluetooth Stack is not Enabled.");
#endif
    }

    return ESP_OK;
}

// MARK: Display
void bsp_tab5_display_set_brightness(int brightness) {
    if (ili9881c) ili9881c_lcd_set_brightness(ili9881c, brightness);
    if (st7123_lcd) st7123_lcd_set_brightness(st7123_lcd, brightness);
}
void *bsp_tab5_display_get_frame_buffer(int fb_index) {
    return frame_buffers[fb_index];
}
void bsp_tab5_display_flush(int fb_index) {
    if (ili9881c) ili9881c_lcd_flush(ili9881c, fb_index);
    if (st7123_lcd) st7123_lcd_flush(st7123_lcd, fb_index);
}

// MARK: Touch Panel
int bsp_tab5_touch_read(esp_lcd_touch_point_data_t *points, uint8_t max_points) {
    if (gt911) return gt911_touch_read(gt911, points, max_points);
    if (st7123_touch) return st7123_touch_read(st7123_touch, points, max_points);
    return 0;
}
void bsp_tab5_touch_wait_interrupt(void) {
    if (gt911) gt911_touch_wait_interrupt(gt911);
}

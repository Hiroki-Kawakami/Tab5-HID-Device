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
#include "soc/clk_tree_defs.h"
#include "pi4io/pi4io.h"
#include "soc/gpio_num.h"
#include "st7123/st7123_lcd.h"
#include "st7123/st7123_touch.h"

#define I2C0_PORT_NUM (0)
static i2c_master_bus_handle_t i2c0;
static pi4io_t pi4ioe1, pi4ioe2;

static void **frame_buffers;
static st7123_lcd_t st7123_lcd;
static st7123_touch_t st7123_touch;

esp_err_t bsp_tab5_init(void) {
    esp_err_t err;

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

    // Initialize ST7123 LCD
    err = st7123_lcd_init(&(st7123_lcd_config_t){
        .backlight_gpio = GPIO_NUM_22,
        .size = (bsp_size_t){ 720, 1280 },
        .pixel_format = BSP_PIXEL_FORMAT_RGB565,
        .fb_num = 1,
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

    return ESP_OK;
}

// MARK: Display
void bsp_tab5_display_set_brightness(int brightness) {
    if (st7123_lcd) st7123_lcd_set_brightness(st7123_lcd, brightness);
}
void *bsp_tab5_display_get_frame_buffer(int fb_index) {
    return frame_buffers[fb_index];
}
void bsp_tab5_display_flush(int fb_index) {
    if (st7123_lcd) st7123_lcd_flush(st7123_lcd, fb_index);
}

// MARK: Touch Panel
int bsp_tab5_touch_read(bsp_point_t *points, uint8_t max_points) {
    if (st7123_touch) return st7123_touch_read(st7123_touch, points, max_points);
    return 0;
}

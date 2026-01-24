/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "display_mux.h"
#include "bsp_tab5.h"
#include "driver/jpeg_types.h"
#include "driver/ppa.h"
#include "driver/jpeg_decode.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "layouts/layout.h"
#include "screens/layout_screen.h"

static const char *TAG = "DisplayMux";
static display_mux_mode_t display_mux_mode;

// MARK: LVGL GUI
#define GUI_SCALE_X       (720.0 / GUI_HEIGHT)
#define GUI_SCALE_Y       (1280.0 / GUI_WIDTH)
#define GUI_BUFFER_SIZE   (GUI_WIDTH * GUI_HEIGHT * 2)

static lv_obj_t *current_lv_screen;
static ppa_client_handle_t gui_ppa;
static void *gui_buffer;
static uint8_t const gui_fb_num = 2;
static uint8_t gui_fb_index = 0;
static lv_indev_t *gui_indev;

static void display_mux_gui_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    if (display_mux_mode != DISPLAY_MUX_MODE_GUI) {
        lv_display_flush_ready(disp);
        return;
    }
    esp_err_t err = ppa_do_scale_rotate_mirror(gui_ppa, &(ppa_srm_oper_config_t){
        .in = {
            .buffer = gui_buffer,
            .pic_w = GUI_WIDTH,
            .pic_h = GUI_HEIGHT,
            .block_w = GUI_WIDTH,
            .block_h = GUI_HEIGHT,
            .block_offset_x = 0,
            .block_offset_y = 0,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = bsp_tab5_display_get_frame_buffer(gui_fb_index),
            .buffer_size = 720 * 1280 * 2,
            .pic_w = 720,
            .pic_h = 1280,
            .block_offset_x = 0,
            .block_offset_y = 0,
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_90,
        .scale_x = GUI_SCALE_X,
        .scale_y = GUI_SCALE_Y,
    });
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to rotate GUI buffer: %s", esp_err_to_name(err));
    }

    bsp_tab5_display_flush(gui_fb_index);
    gui_fb_index = (gui_fb_index + 1) % gui_fb_num;
    lv_display_flush_ready(disp);
}

static void display_mux_gui_input_read(lv_indev_t *indev, lv_indev_data_t *data) {
    esp_lcd_touch_point_data_t point;
    int gui_touch_num = bsp_tab5_touch_read(&point, 1);
    if (gui_touch_num > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = (1280 - point.y) / GUI_SCALE_X;
        data->point.y = point.x / GUI_SCALE_Y;
        return;
    }
    data->state = LV_INDEV_STATE_RELEASED;
}

static void lvgl_setup() {
    lvgl_port_cfg_t config = {
        .task_priority = 4,
        .task_stack = 7168,
        .task_affinity = 0,
        .task_max_sleep_ms = 500,
        .task_stack_caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_DEFAULT,
        .timer_period_ms = 5,
    };
    esp_err_t err = lvgl_port_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start LVGL: %s", esp_err_to_name(err));
        assert(0);
    }
}

static void display_mux_gui_setup(void) {
    lvgl_setup();
    esp_err_t err = ppa_register_client(&(ppa_client_config_t){
        .oper_type = PPA_OPERATION_SRM,
    }, &gui_ppa);
    ESP_ERROR_CHECK(err);
    gui_buffer = heap_caps_malloc(GUI_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    lv_display_t *disp = lv_display_create(GUI_WIDTH, GUI_HEIGHT);
    lv_display_set_buffers(disp, gui_buffer, NULL, GUI_BUFFER_SIZE, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, display_mux_gui_flush);
    bsp_tab5_display_set_brightness(80);

    gui_indev = lv_indev_create();
    lv_indev_set_type(gui_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(gui_indev, display_mux_gui_input_read);
    lv_indev_set_mode(gui_indev, LV_INDEV_MODE_EVENT);

    current_lv_screen = lv_screen_active();
}

void display_mux_gui_screen_load_async(void *screen) {
    lv_screen_load(screen);
    lv_obj_delete(current_lv_screen);
    current_lv_screen = screen;
}
void display_mux_gui_screen_load(lv_obj_t *screen) {
    lv_lock();
    lv_async_call(display_mux_gui_screen_load_async, screen);
    lv_unlock();
}

// MARK: Layout
#define LAYOUT_BUFFER_SIZE (720 * 1280 * 2)
void *display_mux_layout_base_image, *display_mux_layout_active_image;
static jpeg_decoder_handle_t jpeg_decoder;
static ppa_client_handle_t layout_ppa;

static void *display_mux_layout_load_image(const layout_image_t *image, void *buffer) {
    if (!buffer) {
        size_t allocated_size;
        buffer = jpeg_alloc_decoder_mem(LAYOUT_BUFFER_SIZE, &(jpeg_decode_memory_alloc_cfg_t){
            .buffer_direction = JPEG_DEC_ALLOC_OUTPUT_BUFFER,
        }, &allocated_size);
        assert(buffer);
    }

    uint32_t out_size;
    jpeg_decoder_process(jpeg_decoder, &(jpeg_decode_cfg_t){
        .output_format = JPEG_DECODE_OUT_FORMAT_RGB565,
        .rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR,
        .conv_std = JPEG_YUV_RGB_CONV_STD_BT601,
    }, image->data, image->size, buffer, LAYOUT_BUFFER_SIZE, &out_size);

    return buffer;
}

void display_mux_layout_load_images(const layout_image_t *base, const layout_image_t *active) {
    display_mux_layout_base_image = display_mux_layout_load_image(base, display_mux_layout_base_image);
    display_mux_layout_active_image = display_mux_layout_load_image(active, display_mux_layout_active_image);
}

void display_mux_layout_draw_region(const void *image_buffer, uint16_t x, uint16_t y, uint16_t width, uint16_t height) {
    esp_err_t err = ppa_do_scale_rotate_mirror(layout_ppa, &(ppa_srm_oper_config_t){
        .in = {
            .buffer = image_buffer,
            .pic_w = 720,
            .pic_h = 1280,
            .block_w = height,
            .block_h = width,
            .block_offset_x = y,
            .block_offset_y = 1280 - (x + width),
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .out = {
            .buffer = bsp_tab5_display_get_frame_buffer(0),
            .buffer_size = 720 * 1280 * 2,
            .pic_w = 720,
            .pic_h = 1280,
            .block_offset_x = y,
            .block_offset_y = 1280 - (x + width),
            .srm_cm = PPA_SRM_COLOR_MODE_RGB565,
        },
        .rotation_angle = PPA_SRM_ROTATION_ANGLE_0,
        .scale_x = 1,
        .scale_y = 1,
    });
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to draw base image: %s", esp_err_to_name(err));
    }
    bsp_tab5_display_flush(0);
}

static void display_mux_layout_draw_base_image(void) {
    display_mux_layout_draw_region(display_mux_layout_base_image, 0, 0, 1280, 720);
}

static void display_mux_layout_setup(void) {
    esp_err_t err;
    err = jpeg_new_decoder_engine(&(jpeg_decode_engine_cfg_t){
        .intr_priority = 0,
        .timeout_ms = 100,
    }, &jpeg_decoder);
    ESP_ERROR_CHECK(err);

    err = ppa_register_client(&(ppa_client_config_t){
        .oper_type = PPA_OPERATION_SRM,
    }, &layout_ppa);
    ESP_ERROR_CHECK(err);
}

// MARK: Common
void display_mux_switch_mode(display_mux_mode_t mode) {
    display_mux_mode = mode;
    if (mode == DISPLAY_MUX_MODE_LAYOUT) {
        display_mux_layout_draw_base_image();
    }
}

static void trigger_gui_indev_read(void *arg) {
    lv_indev_read(gui_indev);
}
static void display_mux_touch_task(void *param) {
    while (true) {
        bsp_tab5_touch_wait_interrupt();
        if (display_mux_mode == DISPLAY_MUX_MODE_GUI) {
            lv_lock();
            lv_async_call(trigger_gui_indev_read, NULL);
            lv_unlock();
        } else {
            esp_lcd_touch_point_data_t points[5];
            int touch_num = bsp_tab5_touch_read(points, 5);
            for (int i = 0; i < touch_num; i++) {
                uint16_t x = points[i].x, y = points[i].y;
                points[i].x = 1280 - y;
                points[i].y = x;
            }
            layout_screen_on_touch(touch_num, points);
        }
    }
}

void display_mux_setup(void) {
    display_mux_mode = DISPLAY_MUX_MODE_GUI;
    display_mux_gui_setup();
    display_mux_layout_setup();
    xTaskCreatePinnedToCore(display_mux_touch_task, "Touch", 8192, NULL, 20, NULL, 0);
}

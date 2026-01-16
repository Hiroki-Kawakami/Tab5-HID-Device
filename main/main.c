#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "bsp_tab5.h"
#include "driver/ppa.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_lvgl_port.h"
#include "display/lv_display.h"
#include "hal/ppa_types.h"
#include "misc/lv_color.h"

static const char *TAG = "main";

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

#define GUI_WIDTH       (640)
#define GUI_HEIGHT      (360)
#define GUI_BUFFER_SIZE (GUI_WIDTH * GUI_HEIGHT * 2)
static ppa_client_handle_t gui_ppa;
static void *gui_buffer;
static uint8_t const gui_fb_num = 2;
static uint8_t gui_fb_index = 0;
static void gui_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    ppa_srm_oper_config_t config = {
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
        .scale_x = 720.0 / GUI_HEIGHT,
        .scale_y = 1280.0 / GUI_WIDTH,
    };
    esp_err_t err = ppa_do_scale_rotate_mirror(gui_ppa, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to rotate GUI buffer: %s", esp_err_to_name(err));
    }

    bsp_tab5_display_flush(gui_fb_index);
    gui_fb_index = (gui_fb_index + 1) % gui_fb_num;
    lv_display_flush_ready(disp);
}

static void lv_example_get_started_1(void) {
    /*Change the active screen's background color*/
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x003a57), LV_PART_MAIN);

    /*Create a white label, set its text and align it to the center*/
    lv_obj_t * label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello world");
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0xffffff), LV_PART_MAIN);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
}

int app_main() {
    bsp_tab5_init(&(bsp_tab5_config_t){
        .fb_num = gui_fb_num,
    });
    lvgl_setup();

    esp_err_t err = ppa_register_client(&(ppa_client_config_t){
        .oper_type = PPA_OPERATION_SRM,
    }, &gui_ppa);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get PPA handle for LVGL: %s", esp_err_to_name(err));
        assert(0);
    }
    gui_buffer = heap_caps_malloc(GUI_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    lv_display_t *disp = lv_display_create(GUI_WIDTH, GUI_HEIGHT);
    lv_display_set_buffers(disp, gui_buffer, NULL, GUI_BUFFER_SIZE, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, gui_flush);
    bsp_tab5_display_set_brightness(80);

    lv_example_get_started_1();

    while (true) {
        bsp_point_t points[5];
        int touch_num = bsp_tab5_touch_read(points, 5);
        if (touch_num) {
            char str[64] = {}, *sptr = str;
            for (int i = 0; i < touch_num; i++) {
                int step = sprintf(sptr, "(%d, %d) ", points[i].x, points[i].y);
                sptr += step;
            }
            printf("Touch: %s\n", str);
        }

        vTaskDelay(30 / portTICK_PERIOD_MS);
    }
}

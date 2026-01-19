#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bsp_tab5.h"
#include "driver/ppa.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "hid_device.h"
#include "layouts/layout.h"
#include "screens/connect_screen.h"

static const char *TAG = "main";

_layout_context_t *_layout_head;

/*
 * MARK: LVGL
 */
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

#define GUI_WIDTH         (640)
#define GUI_HEIGHT        (360)
#define GUI_SCALE_X       (720.0 / GUI_HEIGHT)
#define GUI_SCALE_Y       (1280.0 / GUI_WIDTH)
#define GUI_BUFFER_SIZE   (GUI_WIDTH * GUI_HEIGHT * 2)
#define GUI_TOUCH_NUM_MAX CONFIG_ESP_LCD_TOUCH_MAX_POINTS
static ppa_client_handle_t gui_ppa;
static void *gui_buffer;
static uint8_t const gui_fb_num = 2;
static uint8_t gui_fb_index = 0;
static int gui_touch_num;
static esp_lcd_touch_point_data_t gui_touches[GUI_TOUCH_NUM_MAX];
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
        .scale_x = GUI_SCALE_X,
        .scale_y = GUI_SCALE_Y,
    };
    esp_err_t err = ppa_do_scale_rotate_mirror(gui_ppa, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to rotate GUI buffer: %s", esp_err_to_name(err));
    }

    bsp_tab5_display_flush(gui_fb_index);
    gui_fb_index = (gui_fb_index + 1) % gui_fb_num;
    lv_display_flush_ready(disp);
}
static void gui_input_read(lv_indev_t *indev, lv_indev_data_t *data) {
    int indev_id = (int)lv_indev_get_user_data(indev);
    if (indev_id == 0) gui_touch_num = bsp_tab5_touch_read(gui_touches, GUI_TOUCH_NUM_MAX);
    for (int i = 0; i < gui_touch_num; i++) {
        if (gui_touches[i].track_id == indev_id) {
            data->state = LV_INDEV_STATE_PRESSED;
            data->point.x = (1280 - gui_touches[i].y) / GUI_SCALE_X;
            data->point.y = gui_touches[i].x / GUI_SCALE_Y;
            return;
        }
    }
    data->state = LV_INDEV_STATE_RELEASED;
}

// Screen Transition
static void update_screen_type(hid_device_state_t current, hid_device_state_t prev) {
    ESP_LOGI(TAG, "update_screen_type: %d->%d", prev, current);
    if (prev == HID_DEVICE_STATE_BEGIN) {
        if (current == HID_DEVICE_STATE_PAIRING) {
            connect_screen_open(lv_screen_active(), &(connect_screen_config_t){
                .mode = CONNECT_SCREEN_MODE_PAIRING,
            });
            return;
        } else if (current == HID_DEVICE_STATE_WAIT_CONNECT) {
            connect_screen_open(lv_screen_active(), &(connect_screen_config_t){
                .mode = CONNECT_SCREEN_MODE_CONNECT,
                .device_name = "Device",
            });
            return;
        }
    }
    if (current == HID_DEVICE_STATE_PAIRING) {
        lv_obj_t *screen = lv_obj_create(NULL);
        connect_screen_open(screen, &(connect_screen_config_t){
            .mode = CONNECT_SCREEN_MODE_PAIRING,
            .cancellable = true,
        });
        lv_screen_load(screen);
    } else if (current == HID_DEVICE_STATE_WAIT_CONNECT) {
        lv_obj_t *screen = lv_obj_create(NULL);
        connect_screen_open(screen, &(connect_screen_config_t){
            .mode = CONNECT_SCREEN_MODE_CONNECT,
            .device_name = "Device",
        });
        lv_screen_load(screen);
    } else if (current == HID_DEVICE_STATE_ACTIVE) {
        lv_obj_t *screen = lv_obj_create(NULL);
        _layout_head->config->build(screen);
        lv_screen_load(screen);
    }
}
static void update_screen_type_async(void *user_data) {
    hid_device_notify_t *notify = user_data;
    update_screen_type(notify->state.current, notify->state.prev);
    lv_free(notify);
}
static void hid_device_notify_callback(hid_device_notify_t *notify, void *user_data) {
    if (notify->type == HID_DEVICE_NOTIFY_STATE_CHANGED) {
        hid_device_notify_t *copy = lv_malloc(sizeof(hid_device_notify_t));
        *copy = *notify;
        lv_async_call(update_screen_type_async, copy);
    }
}

void app_main() {
    bsp_tab5_init(&(bsp_tab5_config_t){
        .display.fb_num = gui_fb_num,
        .bluetooth.enable = true,
    });

    lvgl_setup();

    esp_err_t err = ppa_register_client(&(ppa_client_config_t){
        .oper_type = PPA_OPERATION_SRM,
    }, &gui_ppa);
    ESP_ERROR_CHECK(err);
    gui_buffer = heap_caps_malloc(GUI_BUFFER_SIZE, MALLOC_CAP_SPIRAM);
    lv_display_t *disp = lv_display_create(GUI_WIDTH, GUI_HEIGHT);
    lv_display_set_buffers(disp, gui_buffer, NULL, GUI_BUFFER_SIZE, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(disp, gui_flush);
    bsp_tab5_display_set_brightness(80);

    for (int i = 0; i < GUI_TOUCH_NUM_MAX; i++) {
        lv_indev_t *indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_user_data(indev, (void*)i);
        lv_indev_set_read_cb(indev, gui_input_read);
    }

    // Initialize HID keyboard
    hid_device_add_notify_callback(hid_device_notify_callback, NULL);
    ESP_ERROR_CHECK(hid_device_init(&hid_device_profile_keyboard));
}

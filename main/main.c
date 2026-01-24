#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bsp_tab5.h"
#include "esp_log.h"
#include "hid_device.h"
#include "layouts/layout.h"
#include "screens/connect_screen.h"
#include "display_mux.h"

static const char *TAG = "main";

_layout_context_t *_layout_head;

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
        const layout_config_t *config = _layout_head->config;
        display_mux_layout_load_images(config->base_image, config->active_image);
        display_mux_switch_mode(DISPLAY_MUX_MODE_LAYOUT);
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
        .display.fb_num = GUI_FB_NUM,
        .bluetooth.enable = true,
    });
    display_mux_setup();

    // Initialize HID keyboard
    hid_device_add_notify_callback(hid_device_notify_callback, NULL);
    ESP_ERROR_CHECK(hid_device_init(&hid_device_profile_keyboard));
}

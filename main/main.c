#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "bsp_tab5.h"
#include "esp_log.h"
#include "hid_device.h"
#include "layouts/layout.h"
#include "screens/connect_screen.h"
#include "screens/layout_screen.h"
#include "display_mux.h"

static const char *TAG = "main";

_layout_context_t *_layout_head;

// Screen Transition
static void update_screen_type(hid_device_state_t current, hid_device_state_t prev) {
    ESP_LOGI(TAG, "update_screen_type: %d->%d", prev, current);
    if (current == HID_DEVICE_STATE_PAIRING) {
        connect_screen_open(&(connect_screen_config_t){
            .mode = CONNECT_SCREEN_MODE_PAIRING,
            .cancellable = prev != HID_DEVICE_STATE_BEGIN,
        });
    } else if (current == HID_DEVICE_STATE_WAIT_CONNECT) {
        connect_screen_open(&(connect_screen_config_t){
            .mode = CONNECT_SCREEN_MODE_CONNECT,
            .device_name = "Device",
        });
    } else if (current == HID_DEVICE_STATE_ACTIVE) {
        layout_screen_open(_layout_head->config);
    }
}
static void hid_device_notify_callback(hid_device_notify_t *notify, void *user_data) {
    if (notify->type == HID_DEVICE_NOTIFY_STATE_CHANGED) {
        update_screen_type(notify->state.current, notify->state.prev);
    }
}

void app_main() {
    bsp_tab5_init(&(bsp_tab5_config_t){
        .display.fb_num = GUI_FB_NUM,
        .touch.interrupt = true,
        .bluetooth.enable = true,
    });
    display_mux_setup();

    // Initialize HID keyboard
    hid_device_add_notify_callback(hid_device_notify_callback, NULL);
    ESP_ERROR_CHECK(hid_device_init(&hid_device_profile_keyboard));
}

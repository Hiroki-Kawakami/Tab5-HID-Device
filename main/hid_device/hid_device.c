/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "hid_device.h"
#include "hid_device_keyboard.h"
#include "hid_device_mouse.h"
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_hidd.h"
#include "esp_hid_common.h"

static const char *TAG = "hid_device";

static esp_hidd_dev_t *hid_dev = NULL;

// MARK: Event Message
typedef struct {
    enum {
        HID_DEVICE_MSG_START,
        HID_DEVICE_MSG_START_PAIRING,
        HID_DEVICE_MSG_STOP_PAIRING,
        HID_DEVICE_MSG_CANCEL,
        HID_DEVICE_MSG_CONNECT,
        HID_DEVICE_MSG_DISCONNECT,
        HID_DEVICE_MSG_SEND_REPORT,
    } type;
    union {
        struct {
            uint8_t report_id;
            bool auto_free;
            uint16_t size;
            uint8_t *data;
        } report;
        struct {
            int reason;
        } disconnect;
    };
} hid_device_msg_t;

#define HID_QUEUE_SIZE 16
static QueueHandle_t hid_event_queue = NULL;

void hid_device_push_event_msg(hid_device_msg_t *msg) {
    xQueueSend(hid_event_queue, msg, portMAX_DELAY);
}

// MARK: Notify
#define NOTIFY_CALLBACK_NUM_MAX 8
static struct {
    hid_device_notify_callback_t func;
    void *user_data;
} notify_callbacks[NOTIFY_CALLBACK_NUM_MAX];
static void hid_device_notify(hid_device_notify_t *notify) {
    for (int i = 0; i < NOTIFY_CALLBACK_NUM_MAX; i++) {
        if (notify_callbacks[i].func) notify_callbacks[i].func(notify, notify_callbacks[i].user_data);
    }
}

// MARK: Profile
static const hid_device_profile_t *current_profile;

static const char *profile_get_device_name(void) {
    return current_profile->device_name ?: "M5Stack Tab5";
}
static uint16_t profile_get_appearance(void) {
    uint16_t table[] = {
        [HID_DEVICE_APPEARANCE_GENERIC ] = ESP_HID_APPEARANCE_GENERIC,
        [HID_DEVICE_APPEARANCE_KEYBOARD] = ESP_HID_APPEARANCE_KEYBOARD,
        [HID_DEVICE_APPEARANCE_MOUSE   ] = ESP_HID_APPEARANCE_MOUSE,
        [HID_DEVICE_APPEARANCE_JOYSTICK] = ESP_HID_APPEARANCE_JOYSTICK,
        [HID_DEVICE_APPEARANCE_GAMEPAD ] = ESP_HID_APPEARANCE_GAMEPAD,
    };
    return table[current_profile->appearance];
}
static esp_hid_device_config_t *profile_get_device_config(void) {
    static esp_hid_device_config_t config;
    config.vendor_id = current_profile->vendor_id ?: 0x05AC;
    config.product_id = current_profile->product_id ?: 0x0220;
    config.version = current_profile->version ?: 0x0100;
    config.device_name = profile_get_device_name();
    config.manufacturer_name = current_profile->manufacturer_name ?: "M5Stack";
    config.serial_number = current_profile->serial_number ?: "0000001";

    static esp_hid_raw_report_map_t report_map;
    report_map.data = current_profile->report_map.data;
    report_map.len = current_profile->report_map.size;
    config.report_maps = &report_map;
    config.report_maps_len = 1;
    return &config;
}

// MARK: Bonded Device Storage
static bool get_bonded_device(esp_bd_addr_t addr) {
    int dev_num = esp_ble_get_bond_device_num();
    if (dev_num <= 0) return false;

    esp_ble_bond_dev_t *dev_list = malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    if (!dev_list) return false;

    esp_ble_get_bond_device_list(&dev_num, dev_list);
    if (dev_num > 0) {
        memcpy(addr, dev_list[0].bd_addr, sizeof(esp_bd_addr_t));
        free(dev_list);
        return true;
    }
    free(dev_list);
    return false;
}

// MARK: GAP
static esp_bd_addr_t current_peer_addr;  // Store peer address for passkey/confirm reply

static esp_ble_adv_params_t adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x30,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy  = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        ESP_LOGD(TAG, "GAP ADV data set complete");
        esp_ble_gap_start_advertising(&adv_params);
        break;

    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(TAG, "Advertising started");
        } else {
            ESP_LOGE(TAG, "Advertising start failed: %d", param->adv_start_cmpl.status);
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "Advertising stopped");
        break;

    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(TAG, "Security request from "ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(param->ble_security.ble_req.bd_addr));
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;

    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:
        ESP_LOGI(TAG, "Passkey notify: %06" PRIu32, param->ble_security.key_notif.passkey);
        memcpy(current_peer_addr, param->ble_security.key_notif.bd_addr, sizeof(esp_bd_addr_t));
        hid_device_notify(&(hid_device_notify_t){
            .type = HID_DEVICE_NOTIFY_PASSKEY_DISPLAY,
            .passkey.passkey = param->ble_security.key_notif.passkey,
        });
        break;

    case ESP_GAP_BLE_PASSKEY_REQ_EVT:
        ESP_LOGI(TAG, "Passkey request");
        memcpy(current_peer_addr, param->ble_security.ble_req.bd_addr, sizeof(esp_bd_addr_t));
        hid_device_notify(&(hid_device_notify_t){
            .type = HID_DEVICE_NOTIFY_PASSKEY_INPUT,
        });
        break;

    case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGI(TAG, "Numeric comparison: %06" PRIu32, param->ble_security.key_notif.passkey);
        memcpy(current_peer_addr, param->ble_security.key_notif.bd_addr, sizeof(esp_bd_addr_t));
        hid_device_notify(&(hid_device_notify_t){
            .type = HID_DEVICE_NOTIFY_PASSKEY_CONFIRM,
            .passkey.passkey = param->ble_security.key_notif.passkey,
        });
        break;

    case ESP_GAP_BLE_AUTH_CMPL_EVT:
        if (param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "Authentication complete, addr_type=%d, auth_mode=%d",
                     param->ble_security.auth_cmpl.addr_type,
                     param->ble_security.auth_cmpl.auth_mode);
            hid_device_push_event_msg(&(hid_device_msg_t){ HID_DEVICE_MSG_CONNECT });
        } else {
            ESP_LOGE(TAG, "Authentication failed: 0x%x", param->ble_security.auth_cmpl.fail_reason);
        }
        break;

    default:
        ESP_LOGD(TAG, "GAP event: %d", event);
        break;
    }
}

static void start_pairing(void) {
    ESP_LOGI(TAG, "Starting pairing (undirected advertising)...");

    uint8_t adv_svc_uuid[] = {
        0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
        0x00, 0x10, 0x00, 0x00, 0x12, 0x18, 0x00, 0x00,  // HID Service UUID
    };

    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = true,
        .min_interval = 0x0006,
        .max_interval = 0x0010,
        .appearance = profile_get_appearance(),
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = sizeof(adv_svc_uuid),
        .p_service_uuid = adv_svc_uuid,
        .flag = ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT,
    };

    adv_params.adv_type = ADV_TYPE_IND;
    esp_ble_gap_config_adv_data(&adv_data);
}

static void stop_pairing(void) {
    esp_ble_gap_stop_advertising();
    ESP_LOGI(TAG, "Pairing stopped");
}

static void start_advertise(esp_bd_addr_t addr) {
    ESP_LOGI(TAG, "Starting directed advertising to "ESP_BD_ADDR_STR, ESP_BD_ADDR_HEX(addr));

    adv_params.adv_type = ADV_TYPE_DIRECT_IND_LOW;
    memcpy(adv_params.peer_addr, addr, sizeof(esp_bd_addr_t));
    adv_params.peer_addr_type = BLE_ADDR_TYPE_PUBLIC;

    esp_ble_gap_start_advertising(&adv_params);
}

static void stop_advertise(void) {
    esp_ble_gap_stop_advertising();
    ESP_LOGI(TAG, "Advertising stopped");
}

// MARK: HID Device State
#define HID_DEVICE_STATE_KEEP (HID_DEVICE_STATE_MAX)
static hid_device_state_t current_state = HID_DEVICE_STATE_BEGIN;

static hid_device_state_t state_begin_event_handler(hid_device_msg_t *msg) {
    if (msg->type == HID_DEVICE_MSG_START) {
        esp_bd_addr_t addr;
        if (get_bonded_device(addr)) {
            start_advertise(addr);
            return HID_DEVICE_STATE_WAIT_CONNECT;
        } else {
            start_pairing();
            return HID_DEVICE_STATE_PAIRING;
        }
    }
    return HID_DEVICE_STATE_KEEP;
}
static hid_device_state_t state_wait_connect_event_handler(hid_device_msg_t *msg) {
    if (msg->type == HID_DEVICE_MSG_CONNECT) {
        return HID_DEVICE_STATE_ACTIVE;
    } else if (msg->type == HID_DEVICE_MSG_START_PAIRING) {
        stop_advertise();
        start_pairing();
        return HID_DEVICE_STATE_PAIRING;
    }
    return HID_DEVICE_STATE_KEEP;
}
static hid_device_state_t state_pairing_event_handler(hid_device_msg_t *msg) {
    if (msg->type == HID_DEVICE_MSG_CONNECT) {
        return HID_DEVICE_STATE_ACTIVE;
    } if (msg->type == HID_DEVICE_MSG_STOP_PAIRING) {
        stop_pairing();
    }
    return HID_DEVICE_STATE_KEEP;
}
static hid_device_state_t state_active_event_handler(hid_device_msg_t *msg) {
    if (msg->type == HID_DEVICE_MSG_DISCONNECT) {
        esp_bd_addr_t addr;
        if (get_bonded_device(addr)) {
            start_advertise(addr);
            return HID_DEVICE_STATE_WAIT_CONNECT;
        } else {
            start_pairing();
            return HID_DEVICE_STATE_PAIRING;
        }
    }
    return HID_DEVICE_STATE_KEEP;
}
static hid_device_state_t state_inactive_event_handler(hid_device_msg_t *msg) {
    if (msg->type == HID_DEVICE_MSG_START_PAIRING) {
        start_pairing();
        return HID_DEVICE_STATE_PAIRING;
    }
    return HID_DEVICE_STATE_KEEP;
}
static void state_all_event_handler(hid_device_msg_t *msg) {
    if (msg->type == HID_DEVICE_MSG_SEND_REPORT) {
        if (hid_device_is_connected()) {
            // ESP_LOG_BUFFER_HEX_LEVEL(TAG, msg->report.data, msg->report.size, ESP_LOG_INFO);
            esp_hidd_dev_input_set(hid_dev, 0, msg->report.report_id, msg->report.data, msg->report.size);
        }
        if (msg->report.auto_free) {
            free(msg->report.data);
        }
    }
}

static void hid_device_task(void *param) {
    hid_device_msg_t msg;
    while (true) {
        if (!xQueueReceive(hid_event_queue, &msg, portMAX_DELAY)) {
            continue;
        }
        // ESP_LOGI(TAG, "Recv Msg: event=%d, state=%d", msg.type, current_state);

        typedef hid_device_state_t (*event_handler_t)(hid_device_msg_t *msg);
        const event_handler_t hdlr[] = {
            [HID_DEVICE_STATE_BEGIN       ] = state_begin_event_handler,
            [HID_DEVICE_STATE_WAIT_CONNECT] = state_wait_connect_event_handler,
            [HID_DEVICE_STATE_PAIRING     ] = state_pairing_event_handler,
            [HID_DEVICE_STATE_ACTIVE      ] = state_active_event_handler,
            [HID_DEVICE_STATE_INACTIVE    ] = state_inactive_event_handler,
        };
        hid_device_state_t next_state = hdlr[current_state](&msg);
        state_all_event_handler(&msg);
        if (next_state != HID_DEVICE_STATE_KEEP && current_state != next_state) {
            hid_device_state_t prev_state = current_state;
            current_state = next_state;
            hid_device_notify(&(hid_device_notify_t){
                .type = HID_DEVICE_NOTIFY_STATE_CHANGED,
                .state.prev = prev_state,
                .state.current = current_state,
            });
        }
    }
}

// MARK: ESP_HID
static void hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data) {
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "HID device started");
        hid_device_push_event_msg(&(hid_device_msg_t){ HID_DEVICE_MSG_START });
        break;

    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "HID device connected");
        // hid_device_push_event_msg(&(hid_device_msg_t){ HID_DEVICE_MSG_CONNECT });
        break;

    case ESP_HIDD_PROTOCOL_MODE_EVENT:
        ESP_LOGI(TAG, "Protocol mode: %s",
                 param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
        break;

    case ESP_HIDD_CONTROL_EVENT:
        ESP_LOGI(TAG, "Control: %s",
                 param->control.control ? "EXIT_SUSPEND" : "SUSPEND");
        break;

    case ESP_HIDD_OUTPUT_EVENT:
        ESP_LOGI(TAG, "Output report received, ID: %d, Len: %d",
                 param->output.report_id, param->output.length);
        // TODO: Add callback for output report handling
        break;

    case ESP_HIDD_FEATURE_EVENT:
        ESP_LOGI(TAG, "Feature report received, ID: %d, Len: %d",
                 param->feature.report_id, param->feature.length);
        break;

    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "HID device disconnected, reason: %d", param->disconnect.reason);
        hid_device_push_event_msg(&(hid_device_msg_t){
            .type = HID_DEVICE_MSG_DISCONNECT,
            .disconnect.reason = param->disconnect.reason,
        });
        break;

    case ESP_HIDD_STOP_EVENT:
        ESP_LOGI(TAG, "HID device stopped");
        break;

    default:
        break;
    }
}

// MARK: Bluedroid Init
static esp_err_t init_bluetooth(void) {
    esp_err_t ret;

    // Initialize Bluedroid
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_bluedroid_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_bluedroid_enable failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Set device name
    esp_ble_gap_set_device_name(profile_get_device_name());

    // Register GAP callback
    ret = esp_ble_gap_register_callback(gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gap_register_callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Register GATTS callback (handled by esp_hidd internally)
    extern void esp_hidd_gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
    ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ble_gatts_register_callback failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure security
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
    esp_ble_io_cap_t iocap = ESP_IO_CAP_KBDISP;
    uint8_t key_size = 16;
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint32_t passkey = 0;  // Will be generated randomly
    uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;

    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(auth_req));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(iocap));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(key_size));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(init_key));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(rsp_key));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(passkey));
    esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(auth_option));
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(oob_support));

    return ESP_OK;
}

esp_err_t hid_device_init(const hid_device_profile_t *profile) {
    esp_err_t ret;

    // Create hid_device event queue
    hid_event_queue = xQueueCreate(HID_QUEUE_SIZE, sizeof(hid_device_msg_t));
    if (hid_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create HID queue");
        return ESP_ERR_NO_MEM;
    }

    // Store profile
    current_profile = profile;

    // Initialize Bluetooth
    ret = init_bluetooth();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init Bluetooth: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize HID device
    esp_hid_device_config_t *hid_config = profile_get_device_config();
    ret = esp_hidd_dev_init(hid_config, ESP_HID_TRANSPORT_BLE, hidd_event_callback, &hid_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init HID device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start HID Device Control
    hid_device_keyboard_init();
    hid_device_mouse_init();
    xTaskCreate(hid_device_task, "hid_device", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "HID device initialized (Bluedroid)");
    return ESP_OK;
}

void hid_device_add_notify_callback(hid_device_notify_callback_t callback, void *user_data) {
    for (int i = 0; i < NOTIFY_CALLBACK_NUM_MAX; i++) {
        if (notify_callbacks[i].func == NULL) {
            notify_callbacks[i].func = callback;
            notify_callbacks[i].user_data = user_data;
            return;
        }
    }
    ESP_LOGE(TAG, "Failed to add notify callback, max reached");
}
void hid_device_remove_notify_callback(hid_device_notify_callback_t callback, void *user_data) {
    for (int i = 0; i < NOTIFY_CALLBACK_NUM_MAX; i++) {
        if (notify_callbacks[i].func == callback && notify_callbacks[i].user_data == user_data) {
            notify_callbacks[i].func = NULL;
            notify_callbacks[i].user_data = NULL;
            return;
        }
    }
}
hid_device_state_t hid_device_state(void) {
    return current_state;
}
bool hid_device_is_connected(void) {
    return current_state == HID_DEVICE_STATE_ACTIVE;
}
void hid_device_start_pairing(void) {
    hid_device_push_event_msg(&(hid_device_msg_t){ HID_DEVICE_MSG_START_PAIRING });
}
void hid_device_stop_pairing(void) {
    hid_device_push_event_msg(&(hid_device_msg_t){ HID_DEVICE_MSG_STOP_PAIRING });
}
void hid_device_passkey_input(uint32_t passkey) {
    esp_ble_passkey_reply(current_peer_addr, true, passkey);
}
void hid_device_passkey_confirm(bool accept) {
    esp_ble_confirm_reply(current_peer_addr, accept);
}

void hid_device_send_report(uint8_t report_id, uint8_t *report, uint16_t size, bool auto_free) {
    hid_device_push_event_msg(&(hid_device_msg_t){
        .type = HID_DEVICE_MSG_SEND_REPORT,
        .report = {
            .report_id = report_id,
            .auto_free = auto_free,
            .size = size,
            .data = report,
        },
    });
}

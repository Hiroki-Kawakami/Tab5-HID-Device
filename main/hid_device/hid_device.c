/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "hid_device.h"
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_hidd.h"
#include "esp_hid_common.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

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
    } type;
    union {
        struct {
            uint8_t report_id;
            size_t size;
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
static hid_device_notify_callback_t notify_callbacks[NOTIFY_CALLBACK_NUM_MAX];
static void hid_device_notify(hid_device_notify_t *notify) {
    for (int i = 0; i < NOTIFY_CALLBACK_NUM_MAX; i++) {
        if (notify_callbacks[i]) notify_callbacks[i](notify);
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
    config.serial_number = current_profile->serial_number ?: "0000001"; // temporary

    static esp_hid_raw_report_map_t report_map;
    report_map.data = current_profile->report_map.data;
    report_map.len = current_profile->report_map.size;
    config.report_maps = &report_map;
    config.report_maps_len = 1;
    return &config;
}

// MARK: Peer Storage
bool peer_get_bonded_addr(ble_addr_t *addr) {
    ble_addr_t peer_addrs[16];
    int num_peers;
    int rc = ble_store_util_bonded_peers(peer_addrs, &num_peers, 16);
    if (rc == 0 && num_peers > 0) {
        *addr = peer_addrs[0];
        return true;
    }
    return false;
}

// MARK: GAP
static struct ble_hs_adv_fields adv_fields; // BLE advertisement fields

static int gap_event_handler(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "GAP connection %s, status=%d",
                 event->connect.status == 0 ? "established" : "failed",
                 event->connect.status);
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "GAP disconnected, reason=%d", event->disconnect.reason);
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertising complete, reason=%d", event->adv_complete.reason);
        break;

    case BLE_GAP_EVENT_ENC_CHANGE:
        ESP_LOGI(TAG, "Encryption change, status=%d", event->enc_change.status);
        break;

    case BLE_GAP_EVENT_PASSKEY_ACTION:
        ESP_LOGI(TAG, "Passkey action requested");
        if (event->passkey.params.action == BLE_SM_IOACT_DISP) {
            struct ble_sm_io pkey = {0};
            pkey.action = event->passkey.params.action;
            pkey.passkey = 123456;
            ESP_LOGI(TAG, "Enter passkey %06" PRIu32 " on the peer device", pkey.passkey);
            ble_sm_inject_io(event->passkey.conn_handle, &pkey);
        }
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        struct ble_gap_conn_desc desc;
        ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        ble_store_util_delete_peer(&desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }

    default:
        break;
    }
    return 0;
}

static void start_pairing(void) { // Undirected Connectable
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields rsp_fields;
    int rc;

    // Primary advertisement data (limited to 31 bytes)
    memset(&adv_fields, 0, sizeof(adv_fields));
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.appearance = profile_get_appearance();
    adv_fields.appearance_is_present = 1;

    static ble_uuid16_t hid_uuid = BLE_UUID16_INIT(0x1812);  // HID Service UUID
    adv_fields.uuids16 = &hid_uuid;
    adv_fields.num_uuids16 = 1;
    adv_fields.uuids16_is_complete = 1;

    rc = ble_gap_adv_set_fields(&adv_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set adv fields, rc=%d", rc);
        return;
    }

    // Scan response data (for device name)
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    rsp_fields.name = (uint8_t *)profile_get_device_name();
    rsp_fields.name_len = strlen(profile_get_device_name());
    rsp_fields.name_is_complete = 1;
    rsp_fields.tx_pwr_lvl_is_present = 1;
    rsp_fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set scan rsp fields, rc=%d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    adv_params.itvl_min = BLE_GAP_ADV_ITVL_MS(30);
    adv_params.itvl_max = BLE_GAP_ADV_ITVL_MS(50);

    rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, NULL, BLE_HS_FOREVER,
                           &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start pairing, rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "Pairing started");
}
static void stop_pairing(void) {
    ble_gap_adv_stop();
    ESP_LOGI(TAG, "Pairing stopped");
}

static void start_advertise(ble_addr_t *addr) { // Directed Connectable
    struct ble_gap_adv_params adv_params = {0};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_DIR;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_NON;  // 発見不可

    int rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, addr, BLE_HS_FOREVER,
                            &adv_params, gap_event_handler, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to start advertising, rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising started");
}
static void stop_advertise(void) {
    ble_gap_adv_stop();
    ESP_LOGI(TAG, "Advertising stopped");
}

// MARK: HID Device State
#define HID_DEVICE_STATE_KEEP (HID_DEVICE_STATE_MAX)
static hid_device_state_t current_state = HID_DEVICE_STATE_BEGIN;

static hid_device_state_t state_begin_event_handler(hid_device_msg_t *msg) {
    if (msg->type == HID_DEVICE_MSG_START) {
        ble_addr_t addr;
        if (peer_get_bonded_addr(&addr)) {
            start_advertise(&addr);
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
    if (msg->type == HID_DEVICE_MSG_START) {
        ble_addr_t addr;
        if (peer_get_bonded_addr(&addr)) {
            start_advertise(&addr);
            return HID_DEVICE_STATE_WAIT_CONNECT;
        } else {
            return HID_DEVICE_STATE_INACTIVE;
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

static void hid_device_task(void *param) {
    hid_device_msg_t msg;
    while (true) {
        if (!xQueueReceive(hid_event_queue, &msg, portMAX_DELAY)) {
            continue;
        }
        ESP_LOGI(TAG, "Recv Msg: event=%d, state=%d", msg.type, current_state);

        typedef hid_device_state_t (*event_handler_t)(hid_device_msg_t *msg);
        const event_handler_t hdlr[] = {
            [HID_DEVICE_STATE_BEGIN       ] = state_begin_event_handler,
            [HID_DEVICE_STATE_WAIT_CONNECT] = state_wait_connect_event_handler,
            [HID_DEVICE_STATE_PAIRING     ] = state_pairing_event_handler,
            [HID_DEVICE_STATE_ACTIVE      ] = state_active_event_handler,
            [HID_DEVICE_STATE_INACTIVE    ] = state_inactive_event_handler,
        };
        hid_device_state_t next_state = hdlr[current_state](&msg);
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
        hid_device_push_event_msg(&(hid_device_msg_t){ HID_DEVICE_MSG_CONNECT });
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
        break;

    case ESP_HIDD_FEATURE_EVENT:
        ESP_LOGI(TAG, "Feature report received, ID: %d, Len: %d",
                 param->feature.report_id, param->feature.length);
        break;

    case ESP_HIDD_DISCONNECT_EVENT:
        ESP_LOGI(TAG, "HID device disconnected, reason: %d", param->disconnect.reason);
        hid_device_push_event_msg(&(hid_device_msg_t){
            .type = HID_DEVICE_MSG_CONNECT,
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

// MARK: NimBLE
static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void on_sync(void) {
    ESP_LOGI(TAG, "BLE host synced");

    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to ensure address, rc=%d", rc);
        return;
    }
}

static void on_reset(int reason) {
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

esp_err_t hid_device_init(const hid_device_profile_t *profile) {
    esp_err_t ret;

    // Create hid_device event queue
    hid_event_queue = xQueueCreate(HID_QUEUE_SIZE, sizeof(hid_device_msg_t));
    if (hid_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create HID queue");
        return ESP_ERR_NO_MEM;
    }

    // Configure NimBLE security
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_DISP_ONLY;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_mitm = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ID | BLE_SM_PAIR_KEY_DIST_ENC;
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    // Initialize NVS store
    extern void ble_store_config_init(void);
    ble_store_config_init();

    // Initialize HID device
    current_profile = profile;
    int rc = ble_svc_gap_device_name_set(profile_get_device_name());
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set device name: %d", rc);
        return ESP_FAIL;
    }
    esp_hid_device_config_t *hid_config = profile_get_device_config();
    ret = esp_hidd_dev_init(hid_config, ESP_HID_TRANSPORT_BLE, hidd_event_callback, &hid_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init HID device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    // Start HID message processing task
    xTaskCreate(hid_device_task, "hid_device", 8192, NULL, 5, NULL);

    ESP_LOGI(TAG, "HID device initialized");
    return ESP_OK;
}
void hid_device_add_notify_callback(hid_device_notify_callback_t callback) {
    for (int i = 0; i < NOTIFY_CALLBACK_NUM_MAX; i++) {
        if (notify_callbacks[i] == NULL) {
            notify_callbacks[i] = callback;
            return;
        }
    }
    ESP_LOGE(TAG, "Failed to add notify callback, max reached");
}
void hid_device_remove_notify_callback(hid_device_notify_callback_t callback) {
    for (int i = 0; i < NOTIFY_CALLBACK_NUM_MAX; i++) {
        if (notify_callbacks[i] == callback) {
            notify_callbacks[i] = NULL;
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
void hid_device_start_pairing() {
    hid_device_push_event_msg(&(hid_device_msg_t){ HID_DEVICE_MSG_START_PAIRING });
}
void hid_device_stop_pairing() {
    hid_device_push_event_msg(&(hid_device_msg_t){ HID_DEVICE_MSG_STOP_PAIRING });
}

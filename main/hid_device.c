/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Hiroki Kawakami
 */

#include "hid_device.h"
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_hidd.h"
#include "esp_hid_common.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"

// HID keyboard modifier keys
#define HID_KEY_MOD_LEFT_CTRL   0x01
#define HID_KEY_MOD_LEFT_SHIFT  0x02
#define HID_KEY_MOD_LEFT_ALT    0x04
#define HID_KEY_MOD_LEFT_GUI    0x08
#define HID_KEY_MOD_RIGHT_CTRL  0x10
#define HID_KEY_MOD_RIGHT_SHIFT 0x20
#define HID_KEY_MOD_RIGHT_ALT   0x40
#define HID_KEY_MOD_RIGHT_GUI   0x80

static const char *TAG = "hid_device";

static esp_hidd_dev_t *hid_dev = NULL;
static bool connected = false;
static uint16_t appearance = ESP_HID_APPEARANCE_KEYBOARD;

// Standard HID keyboard report descriptor
// Report format: [modifier, reserved, key1, key2, key3, key4, key5, key6]
static const uint8_t keyboard_report_map[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Key Codes)
    0x19, 0xE0,        //   Usage Minimum (224)
    0x29, 0xE7,        //   Usage Maximum (231)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data, Variable, Absolute) - Modifier byte
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x03,        //   Input (Constant) - Reserved byte
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (1)
    0x29, 0x05,        //   Usage Maximum (5)
    0x91, 0x02,        //   Output (Data, Variable, Absolute) - LED report
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x03,        //   Output (Constant) - Padding
    0x95, 0x06,        //   Report Count (6)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Key Codes)
    0x19, 0x00,        //   Usage Minimum (0)
    0x29, 0x65,        //   Usage Maximum (101)
    0x81, 0x00,        //   Input (Data, Array) - Key array (6 keys)
    0xC0,              // End Collection
};

static esp_hid_raw_report_map_t report_maps[] = {
    {
        .data = keyboard_report_map,
        .len = sizeof(keyboard_report_map),
    },
};

static esp_hid_device_config_t hid_config = {
    .vendor_id = 0x05AC,  // Apple vendor ID for better compatibility
    .product_id = 0x0220,
    .version = 0x0100,
    .device_name = "M5Stack Tab5 Keyboard",
    .manufacturer_name = "M5Stack",
    .serial_number = "0000001",
    .report_maps = report_maps,
    .report_maps_len = 1,
};

// BLE advertisement fields
static struct ble_hs_adv_fields adv_fields;

static void hid_start_advertise(void);

static void hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;

    switch (event) {
    case ESP_HIDD_START_EVENT:
        ESP_LOGI(TAG, "HID device started");
        hid_start_advertise();
        break;

    case ESP_HIDD_CONNECT_EVENT:
        ESP_LOGI(TAG, "HID device connected");
        connected = true;
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
        connected = false;
        hid_start_advertise();
        break;

    case ESP_HIDD_STOP_EVENT:
        ESP_LOGI(TAG, "HID device stopped");
        break;

    default:
        break;
    }
}

static int gap_event_handler(struct ble_gap_event *event, void *arg)
{
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

static void hid_start_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields rsp_fields;
    int rc;

    // Primary advertisement data (limited to 31 bytes)
    memset(&adv_fields, 0, sizeof(adv_fields));
    adv_fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    adv_fields.appearance = appearance;
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
    rsp_fields.name = (uint8_t *)hid_config.device_name;
    rsp_fields.name_len = strlen(hid_config.device_name);
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
        ESP_LOGE(TAG, "Failed to start advertising, rc=%d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising started");
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void on_sync(void)
{
    ESP_LOGI(TAG, "BLE host synced");

    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to ensure address, rc=%d", rc);
        return;
    }

    // esp_hidd_dev_init will trigger START event which starts advertising
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

void ble_store_config_init(void);

esp_err_t hid_device_init(void)
{
    esp_err_t ret;

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
    ble_store_config_init();

    // Initialize HID device
    ret = esp_hidd_dev_init(&hid_config, ESP_HID_TRANSPORT_BLE, hidd_event_callback, &hid_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init HID device: %s", esp_err_to_name(ret));
        return ret;
    }

    // Start NimBLE host task
    nimble_port_freertos_init(ble_host_task);

    ESP_LOGI(TAG, "HID device initialized");
    return ESP_OK;
}

bool hid_device_connected(void)
{
    return connected && esp_hidd_dev_connected(hid_dev);
}

esp_err_t hid_device_send_key(uint8_t modifier, uint8_t keycode)
{
    if (!hid_device_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t report[8] = {0};
    report[0] = modifier;
    report[2] = keycode;

    esp_err_t ret = esp_hidd_dev_input_set(hid_dev, 0, 1, report, sizeof(report));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send key: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t hid_device_release_keys(void)
{
    if (!hid_device_connected()) {
        return ESP_ERR_INVALID_STATE;
    }

    uint8_t report[8] = {0};
    esp_err_t ret = esp_hidd_dev_input_set(hid_dev, 0, 1, report, sizeof(report));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to release keys: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t hid_device_send_char(char c)
{
    uint8_t modifier = 0;
    uint8_t keycode = 0;

    if (c >= 'a' && c <= 'z') {
        keycode = 4 + (c - 'a');
    } else if (c >= 'A' && c <= 'Z') {
        modifier = HID_KEY_MOD_LEFT_SHIFT;
        keycode = 4 + (c - 'A');
    } else if (c >= '1' && c <= '9') {
        keycode = 30 + (c - '1');
    } else if (c == '0') {
        keycode = 39;
    } else if (c == ' ') {
        keycode = 44;
    } else if (c == '\n') {
        keycode = 40;
    }

    if (keycode == 0) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    esp_err_t ret = hid_device_send_key(modifier, keycode);
    if (ret != ESP_OK) {
        return ret;
    }

    vTaskDelay(pdMS_TO_TICKS(20));
    return hid_device_release_keys();
}

esp_err_t hid_device_send_string(const char *str)
{
    if (str == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    while (*str) {
        esp_err_t ret = hid_device_send_char(*str);
        if (ret != ESP_OK && ret != ESP_ERR_NOT_SUPPORTED) {
            return ret;
        }
        str++;
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    return ESP_OK;
}

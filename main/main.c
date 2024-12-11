#include <stdio.h>

#include "esp_log.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "driver/gpio.h"
#include "nvs_flash.h"

#define IN1_PIN GPIO_NUM_1
#define IN2_PIN GPIO_NUM_2
#define ENABLE_PIN GPIO_NUM_4

#define TAG "ESP_IDF_ACTUATOR"
#define SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

static uint8_t motor_speed = 255;
static bool is_motor_running = false;

static esp_gatt_if_t gatts_if = 0;
static uint16_t connection_id = 0;
static uint16_t rx_char_handle = 0;
static uint16_t tx_char_handle = 0;

static void send_ble_notification(const char *message) {
    if (connection_id != 0 && tx_char_handle != 0) {
        esp_ble_gatts_send_indicate(gatts_if, connection_id, tx_char_handle,
                                    strlen(message), (uint8_t *)message, false);
        ESP_LOGI(TAG, "BLE Notification sent: %s", message);
    } else {
        ESP_LOGW(TAG, "No BLE connection to send notification.");
    }
}

static void extend_actuator() {
    is_motor_running = true;
    gpio_set_level(IN1_PIN, 1);
    gpio_set_level(IN2_PIN, 0);
    send_ble_notification("Extending actuator...");
}

static void retract_actuator() {
    is_motor_running = true;
    gpio_set_level(IN1_PIN, 0);
    gpio_set_level(IN2_PIN, 1);
    send_ble_notification("Retracting actuator...");
}

static void stop_actuator() {
    is_motor_running = false;
    gpio_set_level(IN1_PIN, 0);
    gpio_set_level(IN2_PIN, 0);
    send_ble_notification("Actuator stopped.");
}

static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    switch (event) {
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
                ESP_LOGE(TAG, "Advertising start failed");
            } else {
                ESP_LOGI(TAG, "Advertising started");
            }
            break;
        default:
            break;
    }
}

static void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatt_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
        case ESP_GATTS_REG_EVT: {
            esp_gatt_srvc_id_t service_id = {
                .is_primary = true,
                .id.inst_id = 0,
                .id.uuid.len = ESP_UUID_LEN_128,
            };
            memcpy(service_id.id.uuid.uuid.uuid128, SERVICE_UUID, ESP_UUID_LEN_128);
            esp_ble_gatts_create_service(gatt_if, &service_id, 10);
            break;
        }
        case ESP_GATTS_CREATE_EVT:
            esp_ble_gatts_start_service(param->create.service_handle);
            esp_ble_gatts_add_char(param->create.service_handle,
                                   &(esp_bt_uuid_t){.len = ESP_UUID_LEN_128, .uuid.uuid128 = CHARACTERISTIC_UUID_RX},
                                   ESP_GATT_PERM_WRITE,
                                   ESP_GATT_CHAR_PROP_BIT_WRITE, NULL, NULL);
            esp_ble_gatts_add_char(param->create.service_handle,
                                   &(esp_bt_uuid_t){.len = ESP_UUID_LEN_128, .uuid.uuid128 = CHARACTERISTIC_UUID_TX},
                                   ESP_GATT_PERM_READ,
                                   ESP_GATT_CHAR_PROP_BIT_NOTIFY, NULL, NULL);
            break;
        case ESP_GATTS_ADD_CHAR_EVT:
            if (param->add_char.char_uuid.uuid.uuid128[0] == CHARACTERISTIC_UUID_RX[0]) {
                rx_char_handle = param->add_char.attr_handle;
            } else if (param->add_char.char_uuid.uuid.uuid128[0] == CHARACTERISTIC_UUID_TX[0]) {
                tx_char_handle = param->add_char.attr_handle;
            }
            break;
        case ESP_GATTS_WRITE_EVT:
            if (param->write.handle == rx_char_handle) {
                ESP_LOGI(TAG, "Received BLE Write: %.*s", param->write.len, param->write.value);
                if (strncmp((char *)param->write.value, "extend", param->write.len) == 0) {
                    extend_actuator();
                } else if (strncmp((char *)param->write.value, "retract", param->write.len) == 0) {
                    retract_actuator();
                } else if (strncmp((char *)param->write.value, "stop", param->write.len) == 0) {
                    stop_actuator();
                } else {
                    ESP_LOGW(TAG, "Unknown command received.");
                }
            }
            break;
        case ESP_GATTS_CONNECT_EVT:
            connection_id = param->connect.conn_id;
            gatts_if = gatt_if;
            ESP_LOGI(TAG, "Device connected, connection ID: %d", connection_id);
            break;
        case ESP_GATTS_DISCONNECT_EVT:
            connection_id = 0;
            ESP_LOGI(TAG, "Device disconnected");
            esp_ble_gap_start_advertising(&(esp_ble_adv_params_t){
                .adv_int_min = 0x20,
                .adv_int_max = 0x40,
                .adv_type = ADV_TYPE_IND,
                .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
                .channel_map = ADV_CHNL_ALL,
                .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
            });
            break;
        default:
            break;
    }
}

void app_main(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << IN1_PIN) | (1ULL << IN2_PIN) | (1ULL << ENABLE_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&io_conf);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(gatts_event_handler));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(0));

    ESP_LOGI(TAG, "Setup complete. Ready to receive commands.");
}

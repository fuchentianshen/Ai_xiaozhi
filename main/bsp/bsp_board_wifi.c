#include "bsp_board.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

#define TAG "[BSP] WiFi"
#define SECURITY_KEY "12345678"

static void bsp_board_wifi_event_handler(void *event_handler_arg,
                                         esp_event_base_t event_base,
                                         int32_t event_id,
                                         void *event_data)
{
    bsp_board_t *board = (bsp_board_t *)event_handler_arg;
    if ((event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) ||
        (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED))
    {
        ESP_LOGI(TAG, "WiFi connecting...");
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(board->board_status, BSP_BOARD_WIFI_BIT);
    }
    else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_CRED_RECV)
    {
        ESP_LOGW(TAG, "Provisioning failed! Please check the Wi-Fi credentials and try again.");
        wifi_prov_mgr_reset_sm_state_on_failure();
    }
    else if (event_base == WIFI_PROV_EVENT && event_id == WIFI_PROV_END)
    {
        wifi_prov_mgr_deinit();
    }
}

void bsp_board_wifi_init(bsp_board_t *board)
{
    // 检查nvs状态
    bool ret = bsp_board_check_status(board, BSP_BOARD_NVS_BIT, 0);
    if (!ret)
    {
        ESP_LOGW(TAG, "NVS not initialized");
        return;
    }

    // 创建tcp/ip任务
    ESP_ERROR_CHECK(esp_netif_init());

    // 创建event_task
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 注册wifi和ip的事件回调
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &bsp_board_wifi_event_handler, board, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_NETIF_IP_EVENT_GOT_IP, &bsp_board_wifi_event_handler, board, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &bsp_board_wifi_event_handler, board, NULL));

    // 创建wifi任务
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 初始化配网manager
    wifi_prov_mgr_config_t config = {
        .scheme = wifi_prov_scheme_ble,
        .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };
    ESP_ERROR_CHECK(wifi_prov_mgr_init(config));

    // 检查配网状态
    bool provisioned = false;
    wifi_prov_mgr_reset_provisioning();
    // ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
    if (!provisioned)
    {
        // 开始配网
        // 通过mac地址获取默认的服务名称
        uint8_t mac[6] = {0};
        char service_name[15] = {0};
        esp_wifi_get_mac(ESP_IF_WIFI_AP, mac);
        snprintf(service_name, sizeof(service_name), "XIAOZHI_%02X%02X%02X", mac[3], mac[4], mac[5]);
        ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(WIFI_PROV_SECURITY_1, SECURITY_KEY, service_name, NULL));

        // 获取二维码字符串
        char payload[150] = {0};
        snprintf(payload, sizeof(payload), "{\"ver\":\"v1\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"ble\"}", service_name, SECURITY_KEY);
        ESP_LOGI(TAG, "https://espressif.github.io/esp-jumpstart/qrcode.html?data=%s", payload);
    }
    else
    {
        // 配网成功，开始连接wifi
        ESP_LOGI(TAG, "Already provisioned, starting Wi-Fi...");
        // 配置wifi
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        // 启动wifi
        ESP_ERROR_CHECK(esp_wifi_start());
    }
}
#include "ota.h"

#include "object.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "bsp/bsp_board.h"
#include "cJSON.h"
#include "esp_log.h"

#define TAG "OTA"

typedef struct binary
{
    char *data;
    size_t size;
} binary_t;

ota_t *ota_create(void)
{
    ota_t *ota = (ota_t *)object_create(sizeof(ota_t));
    return ota;
}

void ota_destroy(ota_t *ota)
{
    free(ota->activation_code);
    free(ota->websocket_token);
    free(ota->websocket_url);
    free(ota);
}

static esp_err_t ota_http_handler(esp_http_client_event_t *evt)
{
    binary_t *binary = (binary_t *)evt->user_data;
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        // 判断状态
        int status_code = esp_http_client_get_status_code(evt->client);
        if (status_code != 200)
        {
            break;
        }
        // 将收到的数据拼接到binary->data中
        char *new_ptr = realloc(binary->data, binary->size + evt->data_len);
        if (!new_ptr)
        {
            ESP_LOGE(TAG, "realloc failed");
            return ESP_FAIL;
        }
        binary->data = new_ptr;
        memcpy(binary->data + binary->size, evt->data, evt->data_len);
        binary->size += evt->data_len;
        break;
    case HTTP_EVENT_ON_FINISH:
        // 将接收到的数据打印到日志
        ESP_LOGI(TAG, "Received data %.*s", binary->size, binary->data);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static char *ota_get_post_body(void)
{
    // 使用cJSON拼body
    cJSON *root = cJSON_CreateObject();
    bsp_board_t *board = bsp_board_get_instance();

    // 添加application块
    cJSON *application = cJSON_CreateObject();
    cJSON_AddStringToObject(application, "version", "1.0");
    cJSON_AddStringToObject(application, "elf_sha256", "a7d33d0cff3d1e1e9e4b4c34a5c5d5b5e7c7c7c7c7c7c7c7c7c7c7c7c7c7c7c7");

    cJSON_AddItemToObject(root, "application", application);

    // 添加board块
    cJSON *board_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(board_obj, "name", "atguigu-doorbell");
    cJSON_AddStringToObject(board_obj, "type", "atguigu-doorbell");
    cJSON_AddStringToObject(board_obj, "ssid", "abcdef");
    cJSON_AddNumberToObject(board_obj, "rssi", bsp_board_wifi_get_rssi(board));

    cJSON_AddItemToObject(root, "board", board_obj);

    // 转换为字符串
    char *body = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return body;
}

void ota_process(ota_t *ota)
{
    binary_t binary = {0};
    esp_http_client_config_t http_config = {
        .url = OTA_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = ota_http_handler,
        .user_data = &binary,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    esp_http_client_handle_t client = esp_http_client_init(&http_config);
    bsp_board_t *board = bsp_board_get_instance();

    // 添加header
    ESP_ERROR_CHECK(esp_http_client_set_header(client, "Device-Id", board->mac));
    ESP_ERROR_CHECK(esp_http_client_set_header(client, "Client-Id", board->uuid));
    ESP_ERROR_CHECK(esp_http_client_set_header(client, "Accept-language", "zh-CN"));
    ESP_ERROR_CHECK(esp_http_client_set_header(client, "User-Agent", "atguigu-doorbell/1.0"));

    // 添加post body
    char *post_body = ota_get_post_body();
    ESP_ERROR_CHECK(esp_http_client_set_post_field(client, post_body, strlen(post_body)));

    // 发送post请求
    if (esp_http_client_perform(client) != ESP_OK)
    {
        ESP_LOGE("OTA", "Failed to perform HTTP request");
        free(post_body);
        free(binary.data);
        return;
    }

    // 释放掉post body
    free(post_body);

    // 检查状态码
    int status_code = esp_http_client_get_status_code(client);

    // 清理句柄
    ESP_ERROR_CHECK(esp_http_client_cleanup(client));

    if (status_code != 200)
    {
        // 发起警告
        free(binary.data);
        ESP_LOGW("OTA", "OTA failed with status code %d", status_code);
        return;
    }

    // 解析响应
    cJSON *root = cJSON_ParseWithLength(binary.data, binary.size);
    free(binary.data);

    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return;
    }

    free(ota->activation_code);
    ota->activation_code = NULL;
    cJSON *activation_json = cJSON_GetObjectItem(root, "activation_code");
    if (activation_json)
    {
        cJSON *code_json = cJSON_GetObjectItem(activation_json, "code");
        if (cJSON_IsString(code_json))
        {
            ota->activation_code = strdup(code_json->valuestring);
        }
    }

    free(ota->websocket_token);
    ota->websocket_token = NULL;
    free(ota->websocket_url);
    ota->websocket_url = NULL;
    cJSON *websocket_json = cJSON_GetObjectItem(root, "websocket");
    if (websocket_json)
    {
        cJSON *token_json = cJSON_GetObjectItem(websocket_json, "token");
        if (cJSON_IsString(token_json))
        {
            ota->websocket_token = strdup(token_json->valuestring);
        }
        cJSON *url_json = cJSON_GetObjectItem(websocket_json, "url");
        if (cJSON_IsString(url_json))
        {
            ota->websocket_url = strdup(url_json->valuestring);
        }
    }
    cJSON_Delete(root);
}

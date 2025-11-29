#include "protocol.h"
#include "esp_websocket_client.h"
#include "object.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "bsp/bsp_board.h"
#include <stdio.h>
#include "cJSON.h"

#define TAG "Protocol"
#define EVENT_BASE "PEOTCOL_EVENT"

#define protocol_send_text(protocol, fmtstr, ...)                                                 \
    do                                                                                            \
    {                                                                                             \
        if (!esp_websocket_client_is_connected(protocol->ws_client))                              \
        {                                                                                         \
            ESP_LOGE(TAG, "websocket client is not connected");                                   \
            return;                                                                               \
        }                                                                                         \
        char *_text = NULL;                                                                       \
        asprintf(&_text, fmtstr, ##__VA_ARGS__);                                                  \
        esp_websocket_client_send_text(protocol->ws_client, _text, strlen(_text), portMAX_DELAY); \
        free(_text);                                                                              \
    } while (0);

struct protocol
{
    esp_websocket_client_handle_t ws_client; // websocket客户端
    char *session_id;                        // 会话id

    esp_event_handler_t callback; // 回调函数
    void *callback_arg;           // 回调函数参数
};

static void protocol_hello_handler(protocol_t *protocol, cJSON *root)
{
    // 从json中获取会话id
    cJSON *session_id = cJSON_GetObjectItem(root, "session_id");
    if (!cJSON_IsString(session_id))
    {
        ESP_LOGE(TAG, "JSON session_id is not string");
        return;
    }
    free(protocol->session_id);
    protocol->session_id = strdup(session_id->valuestring);
    // 调用回调函数
    protocol->callback(protocol->callback_arg, EVENT_BASE, PROTOCOL_EVENT_HELLO, NULL);
}

static void protocol_llm_handler(protocol_t *protocol, cJSON *root)
{
    // 获取emotion
    cJSON *emotion = cJSON_GetObjectItem(root, "emotion");
    if (!cJSON_IsString(emotion))
    {
        ESP_LOGE(TAG, "JSON emotion is not string");
        return;
    }
    protocol->callback(protocol->callback_arg, EVENT_BASE, PROTOCOL_EVENT_LLM, emotion->valuestring);
}

static void protocol_stt_handler(protocol_t *protocol, cJSON *root)
{
    // 解析text
    cJSON *text = cJSON_GetObjectItem(root, "text");
    if (!cJSON_IsString(text))
    {
        ESP_LOGE(TAG, "JSON text is not string");
        return;
    }
    protocol->callback(protocol->callback_arg, EVENT_BASE, PROTOCOL_EVENT_STT, text->valuestring);
}

static void protocol_tts_handler(protocol_t *protocol, cJSON *root)
{
    // 获取state
    cJSON *state = cJSON_GetObjectItem(root, "state");
    if (!cJSON_IsString(state))
    {
        ESP_LOGE(TAG, "JSON state is not string");
        return;
    }
    if (strcmp(state->valuestring, "start") == 0)
    {
        // 发送事件
        protocol->callback(protocol->callback_arg, EVENT_BASE, PROTOCOL_EVENT_TTS_START, NULL);
    }
    else if (strcmp(state->valuestring, "stop") == 0)
    {
        protocol->callback(protocol->callback_arg, EVENT_BASE, PROTOCOL_EVENT_TTS_STOP, NULL);
    }
    else if (strcmp(state->valuestring, "sentence_start") == 0)
    {
        // 获取text
        cJSON *text = cJSON_GetObjectItem(root, "text");
        if (!cJSON_IsString(text))
        {
            ESP_LOGE(TAG, "JSON text is not string");
            return;
        }
        // 发送事件
        protocol->callback(protocol->callback_arg, EVENT_BASE, PROTOCOL_EVENT_TTS_SENTENCE_START, text->valuestring);
    }
}

static void protocol_ws_event_handler(void *handler_args,
                                      esp_event_base_t base,
                                      int32_t event_id,
                                      void *event_data)
{
    protocol_t *protocol = (protocol_t *)handler_args;
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id)
    {
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGE(TAG, "WEBSOCKET_EVENT_ERROR");
        break;

    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_CONNECTED");
        if (protocol->callback)
        {
            protocol->callback(protocol->callback_arg, EVENT_BASE, PROTOCOL_EVENT_CONNECTED, NULL);
        }
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_DATA");
        // 判断回调函数是否存在
        if (!protocol->callback)
        {
            ESP_LOGW(TAG, "No callback function registered");
            break;
        }

        if (data->op_code != 0x01 && data->op_code != 0x02)
        {
            ESP_LOGW(TAG, "Unhandled data op_code: %d", data->op_code);
            break;
        }
        // 分辨是二进制还是文本
        if (data->op_code == 0x02)
        {
            // 解析二进制数据,代表服务器发来的音频
            binary_data_t audio_data = {
                .data = data->data_ptr,
                .size = data->data_len,
            };
            protocol->callback(protocol->callback_arg, EVENT_BASE, PROTOCOL_EVENT_AUDIO, &audio_data);
        }
        // 解析文本数据
        cJSON *root = cJSON_ParseWithLength(data->data_ptr, data->data_len);
        if (!root)
        {
            ESP_LOGE(TAG, "JSON parse error");
            break;
        }
        cJSON *type = cJSON_GetObjectItem(root, "type");
        if (!cJSON_IsString(type))
        {
            ESP_LOGE(TAG, "JSON type is not string");
            cJSON_Delete(root);
            break;
        }
        if (strcmp(type->valuestring, "hello") == 0)
        {
            // 处理hello消息
            protocol_hello_handler(protocol, root);
        }
        else if (strcmp(type->valuestring, "llm") == 0)
        {
            // 处理llm消息
            protocol_llm_handler(protocol, root);
        }
        else if (strcmp(type->valuestring, "stt") == 0)
        {
            // 处理stt消息
            protocol_stt_handler(protocol, root);
        }
        else if (strcmp(type->valuestring, "tts") == 0)
        {
            // 处理tts消息
            protocol_tts_handler(protocol, root);
        }
        cJSON_Delete(root);
        break;
    case WEBSOCKET_EVENT_CLOSED:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_CLOSED");
        break;
    case WEBSOCKET_EVENT_BEFORE_CONNECT:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_BEFORE_CONNECT");
        break;
    case WEBSOCKET_EVENT_BEGIN:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_BEGIN");
        break;
    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGD(TAG, "WEBSOCKET_EVENT_FINISH");
        if (protocol->callback)
        {
            protocol->callback(protocol->callback_arg, EVENT_BASE, PROTOCOL_EVENT_DISCONNECTED, NULL);
        }
        break;

    default:
        break;
    }
}

protocol_t *protocol_create(const char *url, const char *token)
{
    // 创建peotocol
    protocol_t *protocol = (protocol_t *)object_create(sizeof(protocol_t));
    bsp_board_t *board = bsp_board_get_instance();

    // 初始化websocket客户端， 添加header
    char *headers = NULL;
    asprintf(&headers, "Device-Id: %s\r\nClient-Id: %s\r\nAuthorization: Bearer %s\r\nProtocol-Version: 1\r\n",
             board->mac, board->uuid, token);
    esp_websocket_client_config_t ws_config = {
        .uri = url,
        .headers = headers,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    protocol->ws_client = esp_websocket_client_init(&ws_config);
    esp_websocket_register_events(protocol->ws_client, ESP_EVENT_ANY_ID, protocol_ws_event_handler, protocol);
    free(headers);

    return protocol;
}

void protocol_destroy(protocol_t *protocol)
{
    esp_websocket_client_destroy(protocol->ws_client);
    free(protocol->session_id);
    free(protocol);
}

void protocol_connect(protocol_t *protocol)
{
    if (esp_websocket_client_is_connected(protocol->ws_client))
    {
        ESP_LOGW(TAG, "websocket client is already connected");
        return;
    }
    esp_err_t ret = esp_websocket_client_start(protocol->ws_client);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "websocket client start failed");
    }
}

void protocol_disconnect(protocol_t *protocol)
{
    if (!esp_websocket_client_is_connected(protocol->ws_client))
    {
        ESP_LOGW(TAG, "websocket client is not connected");
        return;
    }
    esp_err_t ret = esp_websocket_client_stop(protocol->ws_client);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "websocket client stop failed");
    }
}

bool protocol_is_connected(protocol_t *protocol)
{
    return esp_websocket_client_is_connected(protocol->ws_client);
}

void protocol_send_hello(protocol_t *protocol)
{
    protocol_send_text(protocol, "{\"audio_params\":{\"channels\":%d,\"format\":\"opus\",\"frame_duration\":60,\"sample_rate\":%d},\"transport\":\"websocket\",\"type\":\"hello\",\"version\":1}", 1, CODEC_SAMPLE_RATE);
}

void protocol_send_wake_word(protocol_t *protocol, const char *wake_word)
{
    protocol_send_text(protocol, "{\"session_id\":\"%s\",\"state\":\"detect\",\"text\":\"%s\",\"type\":\"listen\"}", protocol->session_id, wake_word);
}

void protocol_send_start_listening(protocol_t *protocol, protocol_listening_type_t type)
{
    static const char *mode_str[] = {"auto", "manual", "realtime"};
    protocol_send_text(protocol, "{\"mode\":\"%s\",\"session_id\":\"%s\",\"state\":\"start\",\"type\":\"listen\"}", mode_str[type], protocol->session_id);
}

void protocol_send_stop_listening(protocol_t *protocol)
{
    protocol_send_text(protocol, "{\"session_id\":\"%s\",\"state\":\"stop\",\"type\":\"listen\"}", protocol->session_id);
}

void protocol_send_audio(protocol_t *protocol, binary_data_t *audio_data)
{
    if (!esp_websocket_client_is_connected(protocol->ws_client))
    {
        ESP_LOGE(TAG, "websocket client is not connected");
        return;
    }
    esp_websocket_client_send_bin(protocol->ws_client, audio_data->data, audio_data->size, portMAX_DELAY);
}

void protocol_send_abort_speaking(protocol_t *protocol, protocol_abort_reason_t reason)
{
    static const char *reason_str[] = {"none", "wake_word_detected"};
    protocol_send_text(protocol, "{\"reason\":\"%s\",\"session_id\":\"%s\",\"type\":\"abort\"}", reason_str[reason], protocol->session_id);
}

void protocol_register_callback(protocol_t *protocol, esp_event_handler_t callback, void *arg)
{
    protocol->callback = callback;
    protocol->callback_arg = arg;
}

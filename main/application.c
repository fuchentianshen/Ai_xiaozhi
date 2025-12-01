#include "application.h"
#include "protocol/ota.h"
#include "protocol/protocol.h"
#include "audio/audio_processor.h"
#include "bsp/bsp_board.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "visual/visual_port.h"
#include "visual/visual.h"

#define TAG "Application"

static const char *state_str[] =
    {
        "STARTING",
        "ACTIVATING",
        "IDLE",
        "CONNECTING",
        "WAKEUP",
        "LISTENING",
        "SPEAKING",
};

typedef enum
{
    APP_STATE_STARTING,
    APP_STATE_ACTIVATING,
    APP_STATE_IDLE,
    APP_STATE_CONNECTING,
    APP_STATE_WAKEUP,
    APP_STATE_LISTENING,
    APP_STATE_SPEAKING,
} app_state_t;

typedef struct
{
    app_state_t state;
    protocol_t *protocol;
    audio_processor_t *audio_processor;
    visual_t *visual;

    // 唤醒超时定时器
    esp_timer_handle_t wakeup_timer;
    esp_timer_handle_t visual_timer;
} app_t;

static app_t s_app;

static void application_set_state(app_t *self, app_state_t state)
{
    if (self->state == state)
    {
        return;
    }
    ESP_LOGI(TAG, "State: %s -> %s", state_str[self->state], state_str[state]);
    self->state = state;
    visual_set_state(self->visual, state_str[self->state]);
    if (state == APP_STATE_WAKEUP)
    {
        esp_timer_start_once(self->wakeup_timer, 5 * 1000 * 1000);
    }
    else
    {
        esp_timer_stop(self->wakeup_timer);
    }
}

static void application_check_ota(app_t *self, ota_t *ota)
{
    while (1)
    {
        ota_process(ota);

        // 如果已经激活
        if (!ota->activation_code)
        {
            return;
        }
        application_set_state(self, APP_STATE_ACTIVATING);
        visual_show_notification(self->visual, "激活码", ota->activation_code, 3000);
        ESP_LOGI(TAG, "Activation code: %s", ota->activation_code);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void application_protocol_callback(void *event_handler_arg,
                                          esp_event_base_t event_base,
                                          int32_t event_id,
                                          void *event_data)
{
    app_t *self = (app_t *)event_handler_arg;
    switch (event_id)
    {
    case PROTOCOL_EVENT_CONNECTED: // event_data为NULL
        if (self->state == APP_STATE_CONNECTING)
        {
            protocol_send_hello(self->protocol);
        }
        break;
    case PROTOCOL_EVENT_DISCONNECTED: // event_data为NULL
        audio_processor_set_vad_state(self->audio_processor, false);
        application_set_state(self, APP_STATE_IDLE);
        break;
    case PROTOCOL_EVENT_HELLO: // event_data为NULL
        if (self->state == APP_STATE_CONNECTING)
        {
            protocol_send_wake_word(self->protocol, "你好小智");
            application_set_state(self, APP_STATE_WAKEUP);
        }

        break;
    case PROTOCOL_EVENT_STT: // event_data为char*
        ESP_LOGI(TAG, "User: %s", (char *)event_data);
        visual_set_text(self->visual, (char *)event_data);
        break;
    case PROTOCOL_EVENT_LLM: // event_data为char*
        ESP_LOGI(TAG, "LLM: %s", (char *)event_data);
        visual_set_emotion(self->visual, (char *)event_data);
        break;
    case PROTOCOL_EVENT_TTS_START: // event_data为NULL
        if (self->state == APP_STATE_WAKEUP)
        {
            application_set_state(self, APP_STATE_SPEAKING);
        }
        break;
    case PROTOCOL_EVENT_TTS_SENTENCE_START: // event_data为char*
        if (self->state == APP_STATE_SPEAKING)
        {
            ESP_LOGI(TAG, "Assistant: %s", (char *)event_data);
            visual_set_text(self->visual, (char *)event_data);
        }
        break;
    case PROTOCOL_EVENT_TTS_STOP: // event_data为NULL
        if (self->state == APP_STATE_SPEAKING)
        {
            application_set_state(self, APP_STATE_WAKEUP);
            audio_processor_set_vad_state(self->audio_processor, true);
        }
        break;
    case PROTOCOL_EVENT_AUDIO: // event_data为binary_data_t*
        if (self->state == APP_STATE_SPEAKING)
        {
            binary_data_t *audio_data = (binary_data_t *)event_data;
            audio_processor_write(self->audio_processor, audio_data->data, audio_data->size);
        }
        break;

    default:
        break;
    }
}
static void application_audio_processor_callback(void *event_handler_arg,
                                                 esp_event_base_t event_base,
                                                 int32_t event_id,
                                                 void *event_data)
{
    app_t *self = (app_t *)event_handler_arg;
    switch (event_id)
    {
    case AUDIO_SR_EVENT_WAKEUP:
        // 唤醒
        if (self->state == APP_STATE_IDLE)
        {
            application_set_state(self, APP_STATE_CONNECTING);
            protocol_connect(self->protocol);
        }
        else if (self->state == APP_STATE_SPEAKING)
        {
            protocol_send_abort_speaking(self->protocol, ABORT_REASON_WAKE_WORD);
            protocol_send_wake_word(self->protocol, "你好小智");
            application_set_state(self, APP_STATE_WAKEUP);
        }

        break;
    case AUDIO_SR_EVENT_SILIENCE:
        // 静音
        if (self->state == APP_STATE_LISTENING)
        {
            audio_processor_set_vad_state(self->audio_processor, false);
            application_set_state(self, APP_STATE_WAKEUP);
            protocol_send_stop_listening(self->protocol);
        }

        break;
    case AUDIO_SR_EVENT_SPEECH:
        // 语音
        if (self->state == APP_STATE_WAKEUP)
        {
            protocol_send_start_listening(self->protocol, LISTENING_TYPE_MANUAL);
            application_set_state(self, APP_STATE_LISTENING);
        }

        break;
    default:
        break;
    }
}

static void application_upload_task(void *arg)
{
    app_t *self = (app_t *)arg;
    // 获取opus录音，并上传
    uint8_t buffer[300];
    while (1)
    {
        size_t size = audio_processor_read(self->audio_processor, buffer, sizeof(buffer));
        if (self->state == APP_STATE_LISTENING)
        {
            binary_data_t data = {buffer, size};
            protocol_send_audio(self->protocol, &data);
        }
    }
}

static void application_visual_timer_cb(void *arg)
{
    app_t *app = (app_t *)arg;
    bsp_board_t *board = bsp_board_get_instance();
    int wifi_rssi = bsp_board_wifi_get_rssi(board);
    visual_update(app->visual, 100, wifi_rssi);
}

void application_init(void)
{
    s_app.state = APP_STATE_STARTING;

    bsp_board_t *board = bsp_board_get_instance();
    // 初始化LED和按键
    bsp_board_led_init(board);
    bsp_board_button_init(board);
    bsp_board_lcd_init(board);
    bsp_board_lcd_on(board);
    visual_port_init();

    s_app.visual = visual_create();
    char payload[150] = {0};
    bsp_board_nvs_init(board);
    bsp_board_wifi_init(board, payload, sizeof(payload));
    if (payload[0])
    {
        visual_show_qrcode(s_app.visual, "请扫描二维码配网", payload);
    }

    bsp_board_codec_init(board);

    esp_codec_dev_set_out_vol(board->codec_dev, 60);
    esp_codec_dev_set_in_gain(board->codec_dev, 5);
    esp_codec_dev_sample_info_t sample_info = {
        .bits_per_sample = CODEC_BIT_WIDTH,
        .sample_rate = CODEC_SAMPLE_RATE,
        .channel = 2,
    };
    esp_codec_dev_open(board->codec_dev, &sample_info);

    // 检查初始化状态
    if (bsp_board_check_status(board, BSP_BOARD_LED_BIT | BSP_BOARD_BUTTON_BIT | BSP_BOARD_WIFI_BIT | BSP_BOARD_CODEC_BIT | BSP_BOARD_LCD_BIT, portMAX_DELAY))
    {
        printf("Board initialized successfully!\n");
    }
    else
    {
        printf("Board initialization failed!\n");
    }

    // 关闭wifi二维码
    visual_show_qrcode(s_app.visual, NULL, NULL);

    // 发送ota请求
    ota_t *ota = ota_create();
    application_check_ota(&s_app, ota);
    application_set_state(&s_app, APP_STATE_STARTING);
    s_app.protocol = protocol_create(ota->websocket_url, ota->websocket_token);
    ota_destroy(ota);

    protocol_register_callback(s_app.protocol, application_protocol_callback, &s_app);
    s_app.audio_processor = audio_processor_create();
    audio_processor_register_callback(s_app.audio_processor, AUDIO_SR_EVENT_SILIENCE, application_audio_processor_callback, &s_app);
    audio_processor_register_callback(s_app.audio_processor, AUDIO_SR_EVENT_SPEECH, application_audio_processor_callback, &s_app);
    audio_processor_register_callback(s_app.audio_processor, AUDIO_SR_EVENT_WAKEUP, application_audio_processor_callback, &s_app);

    audio_processor_start(s_app.audio_processor);

    // 启动后台上传任务
    xTaskCreatePinnedToCoreWithCaps(application_upload_task, "upload_task", 4096, &s_app, 6, NULL, 0, MALLOC_CAP_SPIRAM);

    // 初始情况下关闭vad
    audio_processor_set_vad_state(s_app.audio_processor, false);

    // 初始化超时定时器
    esp_timer_create_args_t timer_cfg = {
        .callback = (esp_timer_cb_t)protocol_disconnect,
        .arg = s_app.protocol,
        .skip_unhandled_events = true,
        .name = "wakeup_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_cfg, &s_app.wakeup_timer));

    esp_timer_create_args_t update_timer_cfg = {
        .callback = application_visual_timer_cb,
        .arg = &s_app,
        .skip_unhandled_events = true,
        .name = "visual_timer",
    };
    ESP_ERROR_CHECK(esp_timer_create(&update_timer_cfg, &s_app.visual_timer));
    esp_timer_start_periodic(s_app.visual_timer, 1000 * 1000);

    application_set_state(&s_app, APP_STATE_IDLE);
}
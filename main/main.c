#include <stdio.h>
#include "bsp/bsp_board.h"
#include "esp_log.h"
#include "audio/audio_processer.h"

#define TAG "main"

void button_callback(void *button_handle, void *usr_data)
{
    bsp_board_t *board = bsp_board_get_instance();
    if (button_handle != board->front_button)
    {
        ESP_LOGW(TAG, "button callback error");
        return;
    }

    button_event_t event = iot_button_get_event(button_handle);
    switch (event)
    {
    case BUTTON_SINGLE_CLICK:
        ESP_LOGI(TAG, "button single click");
        led_strip_set_pixel(board->led_strip, 0, 255, 0, 0);
        led_strip_set_pixel(board->led_strip, 1, 0, 0, 255);
        led_strip_refresh(board->led_strip);
        break;
    case BUTTON_DOUBLE_CLICK:
        ESP_LOGI(TAG, "button double click");
        led_strip_clear(board->led_strip);

    default:
        break;
    }
}

void audio_sr_callback(void *event_handler_arg,
                       esp_event_base_t event_base,
                       int32_t event_id,
                       void *event_data)
{
    switch (event_id)
    {
    case AUDIO_SR_EVENT_WAKEUP:
        char *wakeup_word = (char *)event_data;
        ESP_LOGI(TAG, "Wakeup event detected, wakeup word: %s", wakeup_word);
        break;
    case AUDIO_SR_EVENT_SPEECH:
        ESP_LOGI(TAG, "Speech event detected");
        break;
    case AUDIO_SR_EVENT_SILIENCE:
        ESP_LOGI(TAG, "Silence event detected");
        break;

    default:
        ESP_LOGW(TAG, "unknown event");
        break;
    }
}

void app_main(void)
{
    bsp_board_t *board = bsp_board_get_instance();

    // 初始化
    bsp_board_led_init(board);
    bsp_board_button_init(board);
    bsp_board_nvs_init(board);
    // bsp_board_wifi_init(board);
    bsp_board_codec_init(board);
    bsp_board_lcd_init(board);

    // 检查状态
    if (bsp_board_check_status(board,
                               BSP_BOARD_LED_BIT |
                                   BSP_BOARD_BUTTON_BIT |
                                   BSP_BOARD_CODEC_BIT,
                               portMAX_DELAY))
    {
        printf("board init success \n");
    }
    else
    {
        printf("board init failed \n");
    }

    // 注册按键回调
    iot_button_register_cb(board->front_button, BUTTON_SINGLE_CLICK, NULL, button_callback, NULL);
    iot_button_register_cb(board->front_button, BUTTON_DOUBLE_CLICK, NULL, button_callback, NULL);

    // 打开音频设备
    esp_codec_dev_set_out_vol(board->codec_dev, 50);
    esp_codec_dev_set_in_gain(board->codec_dev, 20);
    esp_codec_dev_sample_info_t sample_info = {
        .bits_per_sample = CODEC_BIT_WIDTH,
        .channel = 2,
        .sample_rate = CODEC_SAMPLE_RATE,

    };
    esp_codec_dev_open(board->codec_dev, &sample_info);

    audio_processer_t *processer = audio_processer_create();

    audio_processer_register_callback(processer, AUDIO_SR_EVENT_SILIENCE, audio_sr_callback, NULL);
    audio_processer_register_callback(processer, AUDIO_SR_EVENT_SPEECH, audio_sr_callback, NULL);
    audio_processer_register_callback(processer, AUDIO_SR_EVENT_WAKEUP, audio_sr_callback, NULL);

    audio_processer_start(processer);

    void *buffer = malloc(300);
    while (1)
    {
        size_t size = audio_processer_read(processer, buffer, 300);
        audio_processer_write(processer, buffer, size);
    }
}
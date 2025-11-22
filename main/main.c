#include <stdio.h>
#include "bsp/bsp_board.h"
#include "esp_log.h"

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
        .channel = 1,
        .sample_rate = CODEC_SAMPLE_RATE,

    };
    esp_codec_dev_open(board->codec_dev, &sample_info);
    uint8_t *buffer = (uint8_t *)malloc(1024);
    while (1)
    {
        esp_codec_dev_read(board->codec_dev, buffer, 1024);
        esp_codec_dev_write(board->codec_dev, buffer, 1024);
    }
}
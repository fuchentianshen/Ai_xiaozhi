#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_strip.h"
#include "iot_button.h"
#include "button_adc.h"
#include "esp_log.h"

#define TAG "example"

led_strip_handle_t led_strip_handle = NULL;

static void button_sw2_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "sw2 click");
    // 点亮红灯
    led_strip_set_pixel(led_strip_handle, 0, 255, 0, 0);
    led_strip_refresh(led_strip_handle);
    vTaskDelay(pdMS_TO_TICKS(500));
    led_strip_clear(led_strip_handle);

}
static void button_sw3_cb(void *arg, void *usr_data)
{
    ESP_LOGI(TAG, "sw3 click");
    // 点亮绿灯
    led_strip_set_pixel(led_strip_handle, 1, 0, 255, 0);
    led_strip_refresh(led_strip_handle);
    vTaskDelay(pdMS_TO_TICKS(500));
    led_strip_clear(led_strip_handle);
}

void app_main(void)
{
    led_strip_rmt_config_t led_rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .flags.with_dma = 1,
    };
    led_strip_config_t led_strip_config = {
        .strip_gpio_num = 46,
        .max_leds = 2,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };

    led_strip_new_rmt_device(&led_strip_config, &led_rmt_config, &led_strip_handle);

    button_config_t button_config = {0};
    button_adc_config_t adc_config = {
        .unit_id = ADC_UNIT_1,
        .adc_channel = ADC_CHANNEL_7,
        .button_index = 0,
        .min = 0,
        .max = 400,
    };
    button_handle_t button_handle1 = NULL;

    iot_button_new_adc_device(&button_config, &adc_config, &button_handle1);
    iot_button_register_cb(button_handle1, BUTTON_SINGLE_CLICK, NULL, button_sw2_cb, NULL);
    button_config_t button_config2 = {0};
    button_adc_config_t adc_config2 = {
        .unit_id = ADC_UNIT_1,
        .adc_channel = ADC_CHANNEL_7,
        .button_index = 1,
        .min = 1000,
        .max = 3000,
    };
    button_handle_t button_handle2 = NULL;
    iot_button_new_adc_device(&button_config2, &adc_config2, &button_handle2);
    iot_button_register_cb(button_handle2, BUTTON_SINGLE_CLICK, NULL, button_sw3_cb, NULL);
}
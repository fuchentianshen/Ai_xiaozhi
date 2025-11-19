#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_strip.h"

void app_main(void)
{
    led_strip_handle_t led_strip_handle = NULL;
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

    led_strip_set_pixel(led_strip_handle, 0, 255, 0, 0);// red
    led_strip_set_pixel(led_strip_handle, 1, 0, 255, 0);// green
    led_strip_refresh(led_strip_handle);

    vTaskDelay(5000 / portTICK_PERIOD_MS);

    led_strip_clear(led_strip_handle);
}
#include "bsp_board.h"

void bsp_board_led_init(bsp_board_t *board)
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

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&led_strip_config, &led_rmt_config, &board->led_strip));

    // 设置状态位
    xEventGroupSetBits(board->board_status, BSP_BOARD_LED_BIT);
}
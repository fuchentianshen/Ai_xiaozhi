#include "bsp_board.h"
#include "iot_button.h"
#include "button_adc.h"

void bsp_board_button_init(bsp_board_t *board)
{
    
    button_config_t button_config = {0};
    button_adc_config_t adc_config = {
        .unit_id = ADC_UNIT_1,
        .adc_channel = ADC_CHANNEL_7,
        .button_index = 0,
        .min = 0,
        .max = 20,
    };
    ESP_ERROR_CHECK(iot_button_new_adc_device(&button_config, &adc_config, &board->front_button));

    adc_config.button_index = 1;
    adc_config.min = 1567;
    adc_config.max = 1733;
    ESP_ERROR_CHECK(iot_button_new_adc_device(&button_config, &adc_config, &board->back_button));

    //设置标志位
    xEventGroupSetBits(board->board_status, BSP_BOARD_BUTTON_BIT);
}


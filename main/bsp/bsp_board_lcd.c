#include "bsp_board.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_io.h"

#define LCD_HOST SPI2_HOST

void bsp_board_bk_init(bsp_board_t *board)
{
    gpio_config_t bk_config = {
        .pin_bit_mask = (1ULL << LCD_PIN_BK_LIGHT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&bk_config));
    ESP_ERROR_CHECK(gpio_set_level(LCD_PIN_BK_LIGHT, 0));
}

void bsp_board_spi_init(bsp_board_t *board)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LCD_PIN_MOSI,
        .sclk_io_num = LCD_PIN_PCLK,
        .data1_io_num = -1,
        .data2_io_num = -1,
        .data3_io_num = -1,
        .data4_io_num = -1,
        .data5_io_num = -1,
        .data6_io_num = -1,
        .data7_io_num = -1,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO));
}

void bsp_board_lcd_io_init(bsp_board_t *board)
{
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = LCD_PIN_CS,
        .dc_gpio_num = LCD_PIN_DC,
        .pclk_hz = SPI_MASTER_FREQ_80M,
        .trans_queue_depth = 10,
        .spi_mode = 0,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_config, &board->lcd_io));
}

void bsp_board_lcd_panel_init(bsp_board_t *board)
{
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .color_space = ESP_LCD_COLOR_SPACE_RGB,
        .bits_per_pixel = 16,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(board->lcd_io, &panel_config, &board->lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(board->lcd_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(board->lcd_panel));
}
void bsp_board_lcd_init(bsp_board_t *board)
{
    // 背光引脚初始化
    bsp_board_bk_init(board);

    // spi总线初始化
    bsp_board_spi_init(board);

    // io handle初始化
    bsp_board_lcd_io_init(board);

    // lcd面板初始化
    bsp_board_lcd_panel_init(board);

    // 设置状态位
    xEventGroupSetBits(board->board_status, BSP_BOARD_LCD_BIT);

    // 关闭lcd
    esp_lcd_panel_disp_on_off(board->lcd_panel, false);
}
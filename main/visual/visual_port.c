#include "visual_port.h"
#include "esp_lvgl_port.h"
#include "bsp/bsp_board.h"

static lv_disp_t *disp_handle = NULL;

void visual_port_init(void)
{
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    bsp_board_t *board = bsp_board_get_instance();

    /* Add LCD screen */
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = board->lcd_io,
        .panel_handle = board->lcd_panel,
        .buffer_size = LCD_WIDTH * LCD_HEIGHT,
        .double_buffer = true,
        .hres = LCD_WIDTH,
        .vres = LCD_HEIGHT,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = true,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = true,
            .swap_bytes = false,
        }};
    disp_handle = lvgl_port_add_disp(&disp_cfg);

    /* ... the rest of the initialization ... */
}

void visual_port_deinit(void)
{
    /* If deinitializing LVGL port, remember to delete all displays: */
    lvgl_port_remove_disp(disp_handle);
}

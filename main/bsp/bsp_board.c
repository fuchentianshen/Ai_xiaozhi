#include "bsp_board.h"
#include <stdlib.h>
#include <string.h>
#include "nvs_flash.h"

static bsp_board_t *s_bsp_board_instance = NULL;

bsp_board_t *bsp_board_get_instance(void)
{
    if (s_bsp_board_instance == NULL)
    {
        // 初始化
        s_bsp_board_instance = malloc(sizeof(bsp_board_t));
        memset(s_bsp_board_instance, 0, sizeof(bsp_board_t));

        // 创建状态组
        s_bsp_board_instance->board_status = xEventGroupCreate();
    }

    return s_bsp_board_instance;
}

void bsp_board_nvs_init(bsp_board_t *board)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    xEventGroupSetBits(board->board_status, BSP_BOARD_NVS_BIT);
}

bool bsp_board_check_status(bsp_board_t *board, EventBits_t status_bit, uint32_t timeout_ms)
{
    EventBits_t ret = xEventGroupWaitBits(board->board_status, status_bit, pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms));
    return (ret & status_bit) == status_bit;
}

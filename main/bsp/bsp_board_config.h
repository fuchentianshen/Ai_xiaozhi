#pragma once

#define LED_PIN 46
#define LED_NUM 2

#define BUTTON_PIN 8

// board status
#define BSP_BOARD_LED_BIT (1 << 0)
#define BSP_BOARD_BUTTON_BIT (1 << 1)
#define BSP_BOARD_NVS_BIT (1 << 2)
#define BSP_BOARD_WIFI_BIT (1 << 3)
#define BSP_BOARD_CODEC_BIT (1 << 4)
#define BSP_BOARD_LCD_BIT (1 << 5)

// 编码器引脚
#define CODEC_PA_PIN 7
#define CODEC_SDA_PIN 0
#define CODEC_SCL_PIN 1
#define CODEC_BCK_PIN 2
#define CODEC_MCK_PIN 3
#define CODEC_WS_PIN 5
#define CODEC_DIN_PIN 4
#define CODEC_DOUT_PIN 6

#define CODEC_SAMPLE_RATE 16000
#define CODEC_BIT_WIDTH 16


// LCD
#define LCD_PIN_MOSI           48
#define LCD_PIN_PCLK           47
#define LCD_PIN_CS             21
#define LCD_PIN_DC             45
#define LCD_PIN_RST            16
#define LCD_PIN_BK_LIGHT       40
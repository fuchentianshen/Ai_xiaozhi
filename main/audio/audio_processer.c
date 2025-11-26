#include "audio_processer.h"
#include "audio_encoder.h"
#include "audio_decoder.h"
#include "bsp/bsp_board.h"

#include "object.h"
#include "esp_log.h"

#define TAG "Audio Processer"

#define PLAY_TASK_STACK_SIZE (4 * 1024)
#define PLAY_TASK_CORE_ID 0
#define PLAY_TASK_PRIORITY 5

struct audio_processer
{
    audio_sr_t *sr;
    audio_encoder_t *encoder;
    audio_decoder_t *decoder;

    // 编解码输入输出缓冲区
    RingbufHandle_t enc_input;
    RingbufHandle_t enc_output;
    RingbufHandle_t dec_input;
    RingbufHandle_t dec_output;

    bool is_running;
};

// 播放任务
void audio_processer_paly_task(void *arg)
{
    audio_processer_t *processer = (audio_processer_t *)arg;
    bsp_board_t *board = bsp_board_get_instance();

    while (processer->is_running)
    {
        size_t size = 0;
        void *buffer = xRingbufferReceiveUpTo(processer->dec_output, &size, pdMS_TO_TICKS(50), 2 * 1024);

        if (!buffer)
        {
            continue;
        }

        esp_codec_dev_write(board->codec_dev, buffer, size);
        vRingbufferReturnItem(processer->dec_output, buffer);
    }
    vTaskDelete(NULL);
}

audio_processer_t *audio_processer_create()
{
    audio_processer_t *processer = object_create(sizeof(audio_processer_t));

    processer->enc_input = xRingbufferCreateWithCaps(16 * 1024, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
    processer->enc_output = xRingbufferCreateWithCaps(4 * 1024, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    processer->dec_input = xRingbufferCreateWithCaps(4 * 1024, RINGBUF_TYPE_NOSPLIT, MALLOC_CAP_SPIRAM);
    processer->dec_output = xRingbufferCreateWithCaps(32 * 1024, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);

    processer->encoder = audio_encoder_create(processer->enc_input, processer->enc_output, CODEC_SAMPLE_RATE, CODEC_BIT_WIDTH, 1);
    processer->decoder = audio_decoder_create(processer->dec_input, processer->dec_output, CODEC_SAMPLE_RATE, 2);

    processer->sr = audio_sr_create(processer->enc_input);
    return processer;
}

void audio_processer_destroy(audio_processer_t *processer)
{
    audio_encoder_destroy(processer->encoder);
    audio_decoder_destroy(processer->decoder);
    audio_sr_destroy(processer->sr);

    vRingbufferDelete(processer->enc_input);
    vRingbufferDelete(processer->enc_output);
    vRingbufferDelete(processer->dec_input);
    vRingbufferDelete(processer->dec_output);

    free(processer);
}

void audio_processer_start(audio_processer_t *processer)
{
    processer->is_running = true;
    audio_encoder_start(processer->encoder);
    audio_decoder_start(processer->decoder);
    audio_sr_start(processer->sr);

    xTaskCreatePinnedToCoreWithCaps(audio_processer_paly_task, "play_task",
                                    PLAY_TASK_STACK_SIZE, processer,
                                    PLAY_TASK_PRIORITY, NULL,
                                    PLAY_TASK_CORE_ID, MALLOC_CAP_SPIRAM);
}

void audio_processer_stop(audio_processer_t *processer)
{
    processer->is_running = false;

    audio_encoder_stop(processer->encoder);
    audio_decoder_stop(processer->decoder);
    audio_sr_stop(processer->sr);
}

size_t audio_processer_read(audio_processer_t *processer, void *bufffer, size_t size)
{
    // 从enc_output中读取数据
    size_t read_size = 0;
    void *buf_read = xRingbufferReceive(processer->enc_output, &read_size, portMAX_DELAY);

    if (size < read_size)
    {
        ESP_LOGW(TAG, "缓冲区不足，将数据截断");
        read_size = size;
    }
    memcpy(bufffer, buf_read, read_size);
    vRingbufferReturnItem(processer->enc_output, buf_read);
    return read_size;
}

void audio_processer_write(audio_processer_t *processer, void *bufffer, size_t size)
{
    xRingbufferSend(processer->dec_input, bufffer, size, portMAX_DELAY);
}

void audio_processer_register_callback(audio_processer_t *processer, audio_sr_event_t event, esp_event_handler_t callback, void *arg)
{
    audio_sr_register_callback(processer->sr, event, callback, arg);
}

#include "audio_decoder.h"
#include "esp_audio_dec.h"
#include "esp_audio_dec_default.h"
#include "esp_audio_dec_reg.h"
#include "object.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "[AUDIO] Decoder"

#define AUDIO_DECODER_TASK_STACK_SIZE 32 * 1024
#define AUDIO_DECODER_TASK_PRIORITY 5
#define AUDIO_DECODER_TASK_CORE_ID 0

struct audio_decoder
{
    RingbufHandle_t input_buffer;
    RingbufHandle_t output_buffer;

    esp_audio_dec_handle_t dec;

    bool is_running;
};

void audio_decoder_task(void *arg)
{
    audio_decoder_t *decoder = (audio_decoder_t *)arg;

    esp_audio_dec_out_frame_t out_frame = {
        .buffer = object_create(1024),
        .len = 1024,

    };
    while (decoder->is_running)
    {
        // 从输入缓冲区读取数据
        size_t size_read = 0;
        void *buf_read = xRingbufferReceive(decoder->input_buffer, &size_read, pdMS_TO_TICKS(100));

        esp_audio_dec_in_raw_t in_frame = {
            .buffer = buf_read,
            .len = size_read,
        };

        // 解码
    DEC_PROCESS:
        esp_audio_err_t ret = esp_audio_dec_process(decoder->dec, &in_frame, &out_frame);
        if (ret == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH)
        {
            void *new_buf = heap_caps_realloc(out_frame.buffer, out_frame.needed_size, MALLOC_CAP_SPIRAM);
            if (!new_buf)
            {
                decoder->is_running = false;
                vRingbufferReturnItem(decoder->input_buffer, buf_read);
                break;
            }
            out_frame.buffer = new_buf;
            out_frame.len = out_frame.needed_size;
            goto DEC_PROCESS;
        }
        vRingbufferReturnItem(decoder->input_buffer, buf_read);

        if (ret != ESP_AUDIO_ERR_OK)
        {
            ESP_LOGW(TAG, "Decoder error: %d", ret);
            continue;
        }

        // 写入输出缓冲区
        if (xRingbufferSend(decoder->output_buffer, out_frame.buffer, out_frame.decoded_size, 0) != pdTRUE)
        {
            ESP_LOGW(TAG, "Output buffer full");
        }
    }
    free(out_frame.buffer);
    vTaskDelete(NULL);
}

audio_decoder_t *audio_decoder_create(RingbufHandle_t input_buffer, RingbufHandle_t output_buffer, int sample_rate, int channels)
{
    audio_decoder_t *decoder = object_create(sizeof(audio_decoder_t));
    decoder->input_buffer = input_buffer;
    decoder->output_buffer = output_buffer;

    // 注册解码器
    ESP_ERROR_CHECK(esp_opus_dec_register());

    // 打开解码器
    esp_opus_dec_cfg_t opus_cfg = {
        .sample_rate = sample_rate,
        .channel = channels,
        .frame_duration = ESP_OPUS_DEC_FRAME_DURATION_60_MS,
        .self_delimited = false,
    };
    esp_audio_dec_cfg_t dec_cfg = {
        .type = ESP_AUDIO_TYPE_OPUS,
        .cfg = &opus_cfg,
        .cfg_sz = sizeof(opus_cfg),
    };
    ESP_ERROR_CHECK(esp_audio_dec_open(&dec_cfg, &decoder->dec));
    return decoder;
}

void audio_decoder_destroy(audio_decoder_t *decoder)
{
    esp_audio_dec_close(decoder->dec);
    esp_audio_dec_unregister(ESP_AUDIO_TYPE_OPUS);
    free(decoder);
}

void audio_decoder_start(audio_decoder_t *decoder)
{
    decoder->is_running = true;
    xTaskCreatePinnedToCoreWithCaps(audio_decoder_task,
                                    "decoder_task",
                                    AUDIO_DECODER_TASK_STACK_SIZE,
                                    decoder,
                                    AUDIO_DECODER_TASK_PRIORITY,
                                    NULL,
                                    AUDIO_DECODER_TASK_CORE_ID,
                                    MALLOC_CAP_SPIRAM);
}

void audio_decoder_stop(audio_decoder_t *decoder)
{
    decoder->is_running = false;
}

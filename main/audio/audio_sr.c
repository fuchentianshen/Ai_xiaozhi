#include "audio_sr.h"
#include "esp_afe_sr_models.h"
#include "object.h"
#include "esp_log.h"
#include "bsp/bsp_board.h"

#define TAG "[AUDIO] SR"

#define FEED_TASK_STACK_SIZE 4096
#define FEED_TASK_PRIORITY 5
#define FEED_TASK_CORE 1

#define FETCH_TASK_STACK_SIZE 4096
#define FETCH_TASK_PRIORITY 5
#define FETCH_TASK_CORE 1

ESP_EVENT_DEFINE_BASE(AUDIO_SR_EVENT); // 创建事件

struct audio_sr
{
    bool is_running;

    // sr模型句柄
    const esp_afe_sr_iface_t *afe_handle;
    esp_afe_sr_data_t *afe_data;

    // 处理结果输出缓存
    RingbufHandle_t output;

    // 事件处理句柄
    esp_event_loop_handle_t sr_event_loop;

    // 最近的检测结果
    vad_state_t last_vad_state;
};

// 负责音频数据输入
void audio_sr_feed_task(void *arg)
{
    audio_sr_t *sr = (audio_sr_t *)arg;
    const esp_afe_sr_iface_t *afe_handle = sr->afe_handle;
    esp_afe_sr_data_t *afe_data = sr->afe_data;

    // 计算输入数据的长度
    int feed_chunk_size = afe_handle->get_feed_chunksize(afe_data);
    int feed_nch = afe_handle->get_feed_channel_num(afe_data);
    size_t feed_size = feed_chunk_size * feed_nch * sizeof(int16_t);
    int16_t *feed_buff = (int16_t *)object_create(feed_size);

    // 获取开发板句柄
    bsp_board_t *board = bsp_board_get_instance();

    while (sr->is_running)
    {
        // 从mic中读取数据
        esp_codec_dev_read(board->codec_dev, feed_buff, feed_size);
        // 向afe输入数据
        afe_handle->feed(afe_data, feed_buff);
    }

    free(feed_buff);
    vTaskDelete(NULL);
}

// 负责从afe中获取数据
void audio_sr_fetch_task(void *arg)
{
    audio_sr_t *sr = (audio_sr_t *)arg;
    const esp_afe_sr_iface_t *afe_handle = sr->afe_handle;
    esp_afe_sr_data_t *afe_data = sr->afe_data;

    while (sr->is_running)
    {
        // 从afe中获取数据
        afe_fetch_result_t *result = afe_handle->fetch(afe_data);
        wakenet_state_t wakeup_state = result->wakeup_state;
        if (wakeup_state == WAKENET_DETECTED)
        {
            // 检测到唤醒词，调用回调函数
            esp_event_post_to(sr->sr_event_loop, AUDIO_SR_EVENT, AUDIO_SR_EVENT_WAKEUP, "你好小智", 13, 0);
        }

        // 检测vad状态变化
        vad_state_t vad_state = result->vad_state;
        if (sr->last_vad_state != vad_state)
        {
            sr->last_vad_state = vad_state;
            esp_event_post_to(sr->sr_event_loop,
                              AUDIO_SR_EVENT,
                              vad_state == VAD_SPEECH ? AUDIO_SR_EVENT_SPEECH : AUDIO_SR_EVENT_SILIENCE,
                              NULL, 0, 0);
        }
        if (vad_state == VAD_SPEECH)
        {
            int16_t *processed_audio = result->data;
            int processed_size = result->data_size;

            // 首先向输出缓存放入vad_cache
            if (result->vad_cache_size > 0)
            {
                int16_t *vad_cache = result->vad_cache;
                xRingbufferSend(sr->output, vad_cache, result->vad_cache_size, 0);
            }
            xRingbufferSend(sr->output, processed_audio, processed_size, 0);
        }
    }
    vTaskDelete(NULL);
}

audio_sr_t *audio_sr_create(RingbufHandle_t output)
{
    audio_sr_t *sr = (audio_sr_t *)object_create(sizeof(audio_sr_t));
    sr->output = output;

    // 初始化AFE配置
    srmodel_list_t *models = esp_srmodel_init("model");
    afe_config_t *afe_config = afe_config_init("MR", models, AFE_TYPE_SR, AFE_MODE_HIGH_PERF);

    // 获取句柄
    sr->afe_handle = esp_afe_handle_from_config(afe_config);
    // 创建实例
    sr->afe_data = sr->afe_handle->create_from_config(afe_config);

    free(afe_config);

    // 初始化event句柄
    const esp_event_loop_args_t event_loop_cfg = {
        .queue_size = 10,
        .task_name = "sr_loop",
        .task_priority = 5,
        .task_stack_size = 4096,
        .task_core_id = 0,
    };

    ESP_ERROR_CHECK(esp_event_loop_create(&event_loop_cfg, &sr->sr_event_loop));
    return sr;
}

void audio_sr_destroy(audio_sr_t *sr)
{
    sr->afe_handle->destroy(sr->afe_data);
    free(sr);
}

void audio_sr_start(audio_sr_t *sr)
{
    sr->is_running = true;

    xTaskCreatePinnedToCoreWithCaps(audio_sr_feed_task,
                                    "feed_task",
                                    FEED_TASK_STACK_SIZE,
                                    sr,
                                    FEED_TASK_PRIORITY,
                                    NULL,
                                    FEED_TASK_CORE,
                                    MALLOC_CAP_SPIRAM);
    xTaskCreatePinnedToCoreWithCaps(audio_sr_fetch_task,
                                    "fetch_task",
                                    FETCH_TASK_STACK_SIZE,
                                    sr,
                                    FETCH_TASK_PRIORITY,
                                    NULL,
                                    FETCH_TASK_CORE,
                                    MALLOC_CAP_SPIRAM);
}

void audio_sr_stop(audio_sr_t *sr)
{
    sr->is_running = false;
}

void audio_sr_register_callback(audio_sr_t *sr, audio_sr_event_t event, esp_event_handler_t callback, void *arg)
{
    esp_event_handler_register_with(sr->sr_event_loop, AUDIO_SR_EVENT, event, callback, arg);
}

void audio_sr_set_vad_state(audio_sr_t *sr, bool state)
{
    sr->last_vad_state = VAD_SILENCE;
    if (state)
    {
        sr->afe_handle->enable_vad(sr->afe_data);
    }
    else
    {
        sr->afe_handle->disable_vad(sr->afe_data);
    }
}


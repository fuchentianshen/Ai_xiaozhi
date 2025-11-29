#pragma once
#include "audio_sr.h"

typedef struct audio_processer audio_processer_t;

audio_processer_t *audio_processer_create();

void audio_processer_destroy(audio_processer_t *processer);

void audio_processer_start(audio_processer_t *processer);

void audio_processer_stop(audio_processer_t *processer);

/**
 * @brief 从processer中读取数据,opus编码后的语音
 *
 * @param processer 处理器
 * @param bufffer 缓存区
 * @param size 缓存区大小
 * @return size_t 实际读取的大小
 */
size_t audio_processer_read(audio_processer_t *processer, void *bufffer, size_t size);

/**
 * @brief 向processor写入数据,音频数据
 *
 * @param processer 处理器
 * @param bufffer 缓存区
 * @param size 缓存区大小
 */
void audio_processer_write(audio_processer_t *processer, const void *bufffer, size_t size);

void audio_processer_register_callback(audio_processer_t *processer, audio_sr_event_t event, esp_event_handler_t callback, void *arg);

void audio_processer_set_vad_state(audio_processer_t *processer, bool state);

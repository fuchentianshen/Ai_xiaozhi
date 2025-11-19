#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "task_runner.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "task_runner"

typedef struct
{
    void (*task)(void *);
    void *arg;
} task_item_t; // 任务项

typedef struct task_runner
{
    TaskHandle_t task_runner_task_handle; // 任务运行任务句柄
    bool task_runner_is_running;          // 任务运行状态

    task_item_t *task_list; // 任务列表
    int task_list_count;    // 任务列表数量
    int task_list_capacity; // 任务列表容量
} task_runner_t;

static void task_runner_task(void *arg)
{
    task_runner_t *task_runner = (task_runner_t *)arg;
    // 循环执行任务列表的任务
    while (task_runner->task_runner_is_running)
    {
        // 遍历任务列表
        for (int i = 0; i < task_runner->task_list_count; i++)
        {
            // 执行任务
            task_item_t *task_item = &task_runner->task_list[i];
            if (task_item->task == NULL)
            {
                continue;
            }
            task_item->task(task_item->arg);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    task_runner->task_runner_task_handle = NULL;
    vTaskDelete(NULL);
}

task_runner_handle_t task_runner_init(void)
{
    // 初始化任务运行结构体
    task_runner_t *task_runner = (task_runner_t *)malloc(sizeof(task_runner_t));
    if (!task_runner)
    {
        ESP_LOGE(TAG, "task_runner malloc failed");
        return NULL;
    }
    memset(task_runner, 0, sizeof(task_runner_t));

    // 初始化任务列表
    task_runner->task_list = (task_item_t *)malloc(sizeof(task_item_t) * 10);
    if (!task_runner->task_list)
    {
        ESP_LOGE(TAG, "task_runner_init malloc failed");
        free(task_runner);
        return NULL;
    }
    task_runner->task_list_capacity = 10;
    task_runner->task_list_count = 0;

    return task_runner;
}

void task_runner_start(task_runner_handle_t handle)
{
    task_runner_t *task_runner = (task_runner_t *)handle;
    // 判断任务是否已经开始
    if (task_runner->task_runner_task_handle != NULL)
    {
        ESP_LOGW(TAG, "task_runner_start task is already running");
        return;
    }
    task_runner->task_runner_is_running = true;
    // 启动任务运行任务
    xTaskCreate(task_runner_task, "task_runner_task", 4096, task_runner, 5, &task_runner->task_runner_task_handle);
}

void task_runner_stop(task_runner_handle_t handle)
{
    task_runner_t *task_runner = (task_runner_t *)handle;
    task_runner->task_runner_is_running = false;
    while (task_runner->task_runner_task_handle)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void task_runner_deinit(task_runner_handle_t handle)
{
    task_runner_t *task_runner = (task_runner_t *)handle;
    if (!task_runner)
    {
        return;
    }
    free(task_runner->task_list);
    free(task_runner);
}

void task_runner_add_task(task_runner_handle_t handle,void (*task)(void *), void *arg)
{
    task_runner_t *task_runner = (task_runner_t *)handle;
    // 如果任务在运行，则返回
    if (task_runner->task_runner_is_running)
    {
        ESP_LOGW(TAG, "task_runner_add_task task is already running");
        return;
    }
    // 扩容
    if (task_runner->task_list_count >= task_runner->task_list_capacity)
    {
        int new_capacity = task_runner->task_list_capacity + 10;
        task_item_t *new_list = (task_item_t *)realloc(task_runner->task_list, sizeof(task_item_t) * new_capacity);
        if (!new_list)
        {
            ESP_LOGE(TAG, "task_runner_add_task realloc failed");
            return;
        }
        task_runner->task_list = new_list;
        task_runner->task_list_capacity = new_capacity;
    }
    task_runner->task_list[task_runner->task_list_count].task = task;
    task_runner->task_list[task_runner->task_list_count].arg = arg;
    task_runner->task_list_count++;
}

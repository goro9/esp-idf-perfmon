/* FreeRTOS Real Time Stats Example

   This example code is in the Public Domain (or CC0 licensed, at your option.) 
 
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "stats.h"

#define STATS_TICKS         pdMS_TO_TICKS(1000)
#define STATS_TASK_PRIO     3
#define ARRAY_SIZE_OFFSET   5   //Increase this if print_real_time_stats returns ESP_ERR_INVALID_SIZE
#define ACCUMULATED_INFO_NUM 16

typedef struct {
    char *task_name;
    uint64_t time;
    bool is_running;
} accumulated_info_t;

static const char *TAG = "stats_monitor";
static accumulated_info_t s_accumulated_infos[ACCUMULATED_INFO_NUM];

void stats_reset_accumulated_infos(void) {
    for (int i = 0; i < ACCUMULATED_INFO_NUM; i++) {
        s_accumulated_infos[i].task_name = NULL;
        s_accumulated_infos[i].time = 0;
        s_accumulated_infos[i].is_running = false;
    }
    ESP_LOGI(TAG, "reseted accumulated infos");
}

static void set_accumulated_info(accumulated_info_t *info) {
    uint8_t dst_idx = 255;
    for (int i = 0; i < ACCUMULATED_INFO_NUM; i++) {
        if (s_accumulated_infos[i].task_name == info->task_name) {
            s_accumulated_infos[i].time += info->time;
            s_accumulated_infos[i].is_running = true;
            return;
        }
        else if (s_accumulated_infos[i].task_name == NULL && dst_idx == 255) {
            dst_idx = i;
        }
    }
    if (dst_idx == 255) {
        ESP_LOGE(TAG, "error: accumulated info's buffer is full");
        return;
    }
    s_accumulated_infos[dst_idx].task_name = info->task_name;
    s_accumulated_infos[dst_idx].time = info->time;
    s_accumulated_infos[dst_idx].is_running = true;
}

static accumulated_info_t *get_accumulated_info(const char *task_name) {
    for (int i = 0; i < ACCUMULATED_INFO_NUM; i++) {
        if (s_accumulated_infos[i].task_name == task_name) {
            return &s_accumulated_infos[i];
        }
    }
    return NULL;
}

static void end_calc_accumulated_info(void) {
    for (int i = 0; i < ACCUMULATED_INFO_NUM; i++) {
        if (!s_accumulated_infos[i].is_running) {
            s_accumulated_infos[i].task_name = NULL;
            s_accumulated_infos[i].time = 0;
        }
        else {
            s_accumulated_infos[i].is_running = false;
        }
    }
}

/**
 * @brief   Function to print the CPU usage of tasks over a given duration.
 *
 * This function will measure and print the CPU usage of tasks over a specified
 * number of ticks (i.e. real time stats). This is implemented by simply calling
 * uxTaskGetSystemState() twice separated by a delay, then calculating the
 * differences of task run times before and after the delay.
 *
 * @note    If any tasks are added or removed during the delay, the stats of
 *          those tasks will not be printed.
 * @note    This function should be called from a high priority task to minimize
 *          inaccuracies with delays.
 * @note    When running in dual core mode, each core will correspond to 50% of
 *          the run time.
 *
 * @param   xTicksToWait    Period of stats measurement
 *
 * @return
 *  - ESP_OK                Success
 *  - ESP_ERR_NO_MEM        Insufficient memory to allocated internal arrays
 *  - ESP_ERR_INVALID_SIZE  Insufficient array size for uxTaskGetSystemState. Trying increasing ARRAY_SIZE_OFFSET
 *  - ESP_ERR_INVALID_STATE Delay duration too short
 */
static esp_err_t print_real_time_stats(TickType_t xTicksToWait)
{
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    uint32_t start_run_time, end_run_time;
    esp_err_t ret;

    //Allocate array to store current task states
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = malloc(sizeof(TaskStatus_t) * start_array_size);
    if (start_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get current task states
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    vTaskDelay(xTicksToWait);

    //Allocate array to store tasks states post delay
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = malloc(sizeof(TaskStatus_t) * end_array_size);
    if (end_array == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get post delay task states
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    //Calculate total_elapsed_time in units of run time stats clock period.
    uint32_t total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    printf("| Task | Run Time | Run Time(Accumulated) | Percentage\n");
    printf("| --- | --- | --- | ---\n");
    //Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j;
                //Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
        //Check if matching task found
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * portNUM_PROCESSORS);

            accumulated_info_t buf = {
                .task_name = (char *)start_array[i].pcTaskName,
                .time = task_elapsed_time,
            };
            set_accumulated_info(&buf);
            accumulated_info_t *res = get_accumulated_info(start_array[i].pcTaskName);

            printf("| %s | %d | %lld | %d%%\n", start_array[i].pcTaskName, task_elapsed_time, res->time, percentage_time);
        }
    }

    //Print unmatched tasks
    for (int i = 0; i < start_array_size; i++) {
        if (start_array[i].xHandle != NULL) {
            printf("| %s | Deleted\n", start_array[i].pcTaskName);
        }
    }
    for (int i = 0; i < end_array_size; i++) {
        if (end_array[i].xHandle != NULL) {
            printf("| %s | Created\n", end_array[i].pcTaskName);
        }
    }

    end_calc_accumulated_info();
    ret = ESP_OK;

exit:    //Common return path
    free(start_array);
    free(end_array);
    return ret;
}

static void stats_task(void *arg)
{
    //Print real time stats periodically
    while (1) {
        printf("\n\nGetting real time stats over %d ticks\n", STATS_TICKS);
        if (print_real_time_stats(STATS_TICKS) == ESP_OK) {
            printf("Real time stats obtained\n");
        } else {
            printf("Error getting real time stats\n");
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void stats_init(void) {
    //Create and start stats task
    xTaskCreatePinnedToCore(stats_task, "stats", 4096, NULL, STATS_TASK_PRIO, NULL, tskNO_AFFINITY);
}

static void accumulate_time(int64_t *timer, int64_t start_time, int64_t end_time) {
    int64_t duration = end_time - start_time;
    *timer += duration;
    // ESP_LOGW(TAG, "duration=%lld", duration);
    // ESP_LOGW(TAG, "accumulate time=%lld", *timer);
}

stats_run_time_t *stats_run_time_init(const char* name) {
    stats_run_time_t *buf = malloc(sizeof(stats_run_time_t));
    assert(buf != NULL);
    strcpy(buf->name, name);
    buf->time = 0;
    buf->start = 0;
    buf->state = STATS_MEASURE_STOP;
    return buf;
}

void stats_run_time_start(stats_run_time_t *handler) {
    if (handler == NULL) {
        ESP_LOGE(TAG, "handler is NULL");
        return;
    }
    if (handler->state == STATS_MEASURE_START) {
        ESP_LOGE(TAG, "run time measurement is already started");
        return;
    }
    handler->state = STATS_MEASURE_START;
    handler->start = esp_timer_get_time();
}

void stats_run_time_stop(stats_run_time_t *handler) {
    if (handler == NULL) {
        ESP_LOGE(TAG, "handler is NULL");
        return;
    }
    if (handler->state == STATS_MEASURE_STOP) {
        ESP_LOGE(TAG, "run time measurement is not started");
        return;
    }
    handler->state = STATS_MEASURE_STOP;
    handler->time += esp_timer_get_time() - handler->start;
}

void stats_run_time_free(stats_run_time_t *handler) {
    if (handler == NULL) {
        ESP_LOGE(TAG, "handler is NULL");
        return;
    }
    free(handler);
    handler = NULL;
}

void stats_run_time_print(const stats_run_time_t *handler) {
    if (handler == NULL) {
        ESP_LOGE(TAG, "handler is NULL");
        return;
    }
    ESP_LOGI(TAG, "run time: %s=%lld", handler->name, handler->time);
}
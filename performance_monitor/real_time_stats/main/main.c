/* FreeRTOS Real Time Stats Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "stats_monitor.h"

#define NUM_OF_SPIN_TASKS   6
#define SPIN_ITER           500000  //Actual CPU cycles used will depend on compiler optimization
#define SPIN_TASK_PRIO      2

#ifdef CORE0_ONLY
#define CORE_PINNED (0)
#else
#define CORE_PINNED tskNO_AFFINITY
#endif

static void spin_task(void *arg)
{
    while (1) {
        //Consume CPU cycles
        for (int i = 0; i < SPIN_ITER; i++) {
            __asm__ __volatile__("NOP");
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main()
{
    //Allow other core to finish initialization
    vTaskDelay(pdMS_TO_TICKS(100));

    char task_names[NUM_OF_SPIN_TASKS][configMAX_TASK_NAME_LEN];

    //Create spin tasks
    for (int i = 0; i < NUM_OF_SPIN_TASKS; i++) {
        snprintf(task_names[i], configMAX_TASK_NAME_LEN, "spin%d", i);
        xTaskCreatePinnedToCore(spin_task, task_names[i], 1024, NULL, SPIN_TASK_PRIO, NULL, CORE_PINNED);
    }

    //Create and start stats task
    // xTaskCreatePinnedToCore(stats_task, "stats", 4096, NULL, STATS_TASK_PRIO, NULL, tskNO_AFFINITY);
    stats_monitor_init();
}

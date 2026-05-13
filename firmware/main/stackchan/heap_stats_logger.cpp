/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "heap_stats_logger.h"

#include <esp_heap_caps.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "HeapDiag"

namespace stackchan::diag {

static void heap_stats_task(void* /*arg*/)
{
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000));

        const size_t int_free     = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        const size_t int_min      = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
        const size_t dma_free     = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        const size_t dma_min      = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        const size_t psram_free   = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        const size_t psram_min    = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
        const size_t psram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

        ESP_LOGI(TAG,
                 "internal free=%u min=%u | int+DMA free=%u min=%u | psram free=%u min=%u largest=%u",
                 (unsigned)int_free, (unsigned)int_min, (unsigned)dma_free, (unsigned)dma_min,
                 (unsigned)psram_free, (unsigned)psram_min, (unsigned)psram_largest);
    }
}

void start_heap_stats_logger()
{
    constexpr UBaseType_t kPrio = 3;
    constexpr BaseType_t kCore  = 1;
    static bool started         = false;
    if (started) {
        return;
    }
    started = true;
    xTaskCreatePinnedToCore(heap_stats_task, "heap_diag", 3072, nullptr, kPrio, nullptr, kCore);
}

}  // namespace stackchan::diag

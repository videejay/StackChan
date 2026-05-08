/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

/**
 * M5Stack CoreS3 routes both the ILI9342 LCD (LVGL flush) and the microSD
 * (SDSPI) through SPI3_HOST. Concurrent SPI transactions from different tasks
 * can hit spi_hal_setup_trans asserts. Serialize SD VFS access with the same
 * mutex LVGL uses for panel I/O (esp_lvgl_port_lock).
 */
#include <esp_lvgl_port.h>
#include <esp_log.h>

struct StackchanSpi3LvglLockGuard {
    bool locked = false;
    StackchanSpi3LvglLockGuard(const char* reason, int timeout_ms = 3000)
    {
        locked = lvgl_port_lock(timeout_ms);
        if (!locked) {
            ESP_LOGW("SPI3", "lvgl_port_lock failed (%s) — SPI3 may contend with LCD", reason ? reason : "?");
        }
    }
    ~StackchanSpi3LvglLockGuard()
    {
        if (locked) {
            lvgl_port_unlock();
        }
    }
    StackchanSpi3LvglLockGuard(const StackchanSpi3LvglLockGuard&)            = delete;
    StackchanSpi3LvglLockGuard& operator=(const StackchanSpi3LvglLockGuard&) = delete;
};

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void start_ota_update(const char* url, void (*on_progress)(int progress));

/**
 * With CONFIG_STACKCHAN_SD_UI_ASSETS: download firmware to SD
 * (<mount>/apps/<basename>.bin), flash via esp_ota, reboot.
 * basename should be filesystem-safe; NULL or empty derives from URL.
 * Without SD UI assets: same as start_ota_update().
 */
void start_ota_update_cached_on_sd(const char* url, const char* storage_basename,
                                   void (*on_progress)(int progress));

#ifdef __cplusplus
}
#endif

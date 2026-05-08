/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Lookup embedded fallback launcher UI asset bytes (always succeeds for bundled manifest names). */
bool stackchan_launcher_embedded_lookup(const char* name, const uint8_t** out_data, size_t* out_length);

/**
 * Mount SD (and LittleFS launcher partition if configured) once, same as early launcher init.
 * Safe to call repeatedly. Used by App Store OTA before writing under the SD mount.
 */
void stackchan_ensure_sd_mounted(void);

#ifdef __cplusplus
}
#endif

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

#ifdef __cplusplus
}
#endif

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace stackchan_assets {

enum class LauncherAssetSource : uint8_t {
    Embedded,
    LittleFS,
};

/** LittleFS-first (when enabled + mounted), else embedded blobs. Returned pointer stays valid thanks to filesystem cache vectors. */
bool get_launcher_ui_asset_bytes(std::string_view name, const uint8_t** out_ptr, std::size_t* out_len,
                                 LauncherAssetSource* out_src = nullptr);

/** Call once early in boot — mounts optional LittleFS (Kconfig). */
void early_init_launcher_assets();

}  // namespace stackchan_assets

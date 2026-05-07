/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "assets.h"
#include "stackchan_asset_provider.hpp"
#include <cstring>
#include <mooncake_log.h>

static const std::string_view _tag = "Assets";

namespace assets {

static bool has_suffix(std::string_view str, std::string_view suffix)
{
    return str.size() >= suffix.size() &&
           str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
}

lv_image_dsc_t get_image(std::string_view name)
{
    lv_image_dsc_t dsc = {};

    const uint8_t* raw = nullptr;
    std::size_t raw_len = 0;

    // LittleFS overrides (Launcher) then embedded flash fallback — no mmap assets partition.
    if (!stackchan_assets::get_launcher_ui_asset_bytes(name, &raw, &raw_len)) {
        return dsc;
    }

    void* data_ptr       = reinterpret_cast<void*>(const_cast<uint8_t*>(raw));
    const size_t data_size = raw_len;

    if (has_suffix(name, ".bin")) {
        if (data_size > sizeof(lv_image_header_t)) {
            memcpy(&dsc.header, data_ptr, sizeof(lv_image_header_t));
            dsc.data_size = data_size - sizeof(lv_image_header_t);
            dsc.data      = static_cast<const uint8_t*>(data_ptr) + sizeof(lv_image_header_t);
        } else {
            mclog::tagError(_tag, "bin asset {} size too small", name);
        }
    } else if (has_suffix(name, ".png") || has_suffix(name, ".jpg") || has_suffix(name, ".jpeg") ||
               has_suffix(name, ".gif")) {
        dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        dsc.header.cf    = LV_COLOR_FORMAT_RAW_ALPHA;
        dsc.data_size    = data_size;
        dsc.data         = static_cast<const uint8_t*>(data_ptr);
    } else {
        mclog::tagWarn(_tag, "unknown asset type for {}, treating as raw", name);
        dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        dsc.header.cf    = LV_COLOR_FORMAT_RAW;
        dsc.data_size    = data_size;
        dsc.data         = static_cast<const uint8_t*>(data_ptr);
    }

    return dsc;
}

}  // namespace assets

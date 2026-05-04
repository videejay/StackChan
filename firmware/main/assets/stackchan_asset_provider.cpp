/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "stackchan_asset_provider.hpp"
#include "stackchan_asset_provider.h"

#include <sdkconfig.h>

#include <cstring>
#include <cstdio>

#include <mooncake_log.h>

#if CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS
#include <esp_err.h>
#include <esp_log.h>
#include <esp_littlefs.h>
#endif

namespace {

#if CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS
static constexpr const char* TAG_LFS = "stackchan_lfs";
#endif

std::unordered_map<std::string, std::vector<uint8_t>> s_fs_cache;
std::mutex s_cache_mtx;
static constexpr std::string_view LOG_TAG = "stackchan_assets";

#if CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS
static bool s_lfs_attempted = false;
static bool s_lfs_ready = false;

static std::string make_lfs_path(std::string_view name)
{
    std::string base = CONFIG_STACKCHAN_LITTLEFS_MOUNT_PATH;
    const char* rel = CONFIG_STACKCHAN_LITTLEFS_ASSETS_SUBPATH;

    while (!base.empty() && base.back() == '/') {
        base.pop_back();
    }

    std::string out = base;

    bool has_rel = rel != nullptr && rel[0] != '\0';
    if (has_rel) {
        if (!base.empty()) {
            out.push_back('/');
        }
        while (rel[0] == '/') {
            rel++;
        }
        out.append(rel);
    }

    if (!out.empty()) {
        out.push_back('/');
    }
    out.append(name.begin(), name.end());
    return out;
}

static void mount_lfs_once(void)
{
    if (s_lfs_attempted) {
        return;
    }
    s_lfs_attempted = true;

    esp_vfs_littlefs_conf_t conf{};
    conf.base_path = CONFIG_STACKCHAN_LITTLEFS_MOUNT_PATH;
    conf.partition_label = CONFIG_STACKCHAN_LITTLEFS_PARTITION_LABEL;
#ifdef CONFIG_STACKCHAN_LITTLEFS_FORMAT_IF_MOUNT_FAILED
    conf.format_if_mount_failed = true;
#else
    conf.format_if_mount_failed = false;
#endif
#ifdef CONFIG_STACKCHAN_LITTLEFS_MOUNT_READ_ONLY
    conf.read_only = true;
#else
    conf.read_only = false;
#endif
    conf.dont_mount = false;
    conf.grow_on_mount = false;

    esp_err_t err = esp_vfs_littlefs_register(&conf);
    if (err == ESP_OK) {
        s_lfs_ready = true;
        ESP_LOGI(TAG_LFS, "mounted label=%s at %s", conf.partition_label, conf.base_path);
    } else {
        ESP_LOGW(TAG_LFS, "LittleFS mount failed (%s) label='%s'",
                 esp_err_to_name(err),
                 CONFIG_STACKCHAN_LITTLEFS_PARTITION_LABEL);
        s_lfs_ready = false;
    }
}

static bool read_lfs_into_cache_locked(const std::string& key)
{
    if (!s_lfs_ready || key.empty()) {
        return false;
    }

    std::string path = make_lfs_path(key);

    FILE* f = fopen(path.c_str(), "rb");
    if (!f) {
        return false;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }

    long sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        return false;
    }
    rewind(f);

    std::vector<uint8_t> buf(static_cast<size_t>(sz));
    size_t nr = fread(buf.data(), 1, buf.size(), f);
    fclose(f);
    if (nr != buf.size()) {
        return false;
    }

    s_fs_cache.emplace(std::piecewise_construct, std::forward_as_tuple(key), std::forward_as_tuple(std::move(buf)));
    return true;
}
#endif /* CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS */

}  // namespace

namespace stackchan_assets {

void early_init_launcher_assets()
{
#if CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS
    mount_lfs_once();
#endif
}

bool get_launcher_ui_asset_bytes(std::string_view name, const uint8_t** out_ptr, std::size_t* out_len,
                                 LauncherAssetSource* out_src)
{
    if (out_ptr == nullptr || out_len == nullptr || name.empty()) {
        return false;
    }

    const std::string key(name);

#if CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS
    {
        std::lock_guard<std::mutex> guard(s_cache_mtx);

        mount_lfs_once();

        auto it = s_fs_cache.find(key);
        if (it == s_fs_cache.end()) {
            read_lfs_into_cache_locked(key);
            it = s_fs_cache.find(key);
        }

        if (it != s_fs_cache.end()) {
            *out_ptr = it->second.data();
            *out_len = it->second.size();
            if (out_src) {
                *out_src = LauncherAssetSource::LittleFS;
            }
            return true;
        }
    }
#endif

    const uint8_t* emb = nullptr;
    size_t emblen = 0;
    if (stackchan_launcher_embedded_lookup(key.c_str(), &emb, &emblen) && emb != nullptr && emblen > 0) {
        *out_ptr = emb;
        *out_len = emblen;
        if (out_src) {
            *out_src = LauncherAssetSource::Embedded;
        }
        return true;
    }

    mclog::tagError(LOG_TAG, "launcher asset '{}' not found", key);
    return false;
}

}  // namespace stackchan_assets

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "stackchan_asset_provider.hpp"
#include "stackchan_asset_provider.h"

#include <sdkconfig.h>

#include <cstdio>
#include <cstring>

#include <mooncake_log.h>

#ifdef CONFIG_STACKCHAN_SD_UI_ASSETS
#include <driver/sdspi_host.h>
#include <driver/spi_common.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>
#endif

#if CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS
#include <esp_err.h>
#include <esp_log.h>
#include <esp_littlefs.h>
#endif

namespace {

#if CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS
static constexpr const char* TAG_LFS = "stackchan_lfs";
#endif
#ifdef CONFIG_STACKCHAN_SD_UI_ASSETS
static constexpr const char* TAG_SD = "stackchan_sd";
#endif

std::unordered_map<std::string, std::vector<uint8_t>> s_fs_cache;
std::mutex s_cache_mtx;
static constexpr std::string_view LOG_TAG = "stackchan_assets";

static std::string make_source_cache_key(const char* source, const std::string& name)
{
    std::string key(source);
    key.push_back(':');
    key.append(name);
    return key;
}

static std::string make_asset_path(const char* base_path, const char* assets_subpath, std::string_view name)
{
    std::string base = base_path == nullptr ? "" : base_path;
    const char* rel = assets_subpath;

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

static bool read_file_into_cache_locked(const std::string& cache_key, const std::string& path)
{
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

    s_fs_cache.emplace(std::piecewise_construct, std::forward_as_tuple(cache_key), std::forward_as_tuple(std::move(buf)));
    return true;
}

#ifdef CONFIG_STACKCHAN_SD_UI_ASSETS
static bool s_sd_attempted = false;
static bool s_sd_ready = false;
static sdmmc_card_t* s_sd_card = nullptr;

static std::string make_sd_path(std::string_view name)
{
    return make_asset_path(CONFIG_STACKCHAN_SD_MOUNT_PATH, CONFIG_STACKCHAN_SD_ASSETS_SUBPATH, name);
}

static void mount_sd_once(void)
{
    if (s_sd_attempted) {
        return;
    }
    s_sd_attempted = true;

    spi_bus_config_t bus_config{};
    bus_config.mosi_io_num = static_cast<gpio_num_t>(CONFIG_STACKCHAN_SD_SPI_MOSI_GPIO);
    bus_config.miso_io_num = static_cast<gpio_num_t>(CONFIG_STACKCHAN_SD_SPI_MISO_GPIO);
    bus_config.sclk_io_num = static_cast<gpio_num_t>(CONFIG_STACKCHAN_SD_SPI_SCLK_GPIO);
    bus_config.quadwp_io_num = GPIO_NUM_NC;
    bus_config.quadhd_io_num = GPIO_NUM_NC;
    bus_config.max_transfer_sz = 320 * 240 * sizeof(uint16_t);

    esp_err_t err = spi_bus_initialize(SPI3_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG_SD, "SPI bus init failed (%s)", esp_err_to_name(err));
        s_sd_ready = false;
        return;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.host_id = static_cast<spi_host_device_t>(host.slot);
    slot_config.gpio_cs = static_cast<gpio_num_t>(CONFIG_STACKCHAN_SD_SPI_CS_GPIO);

    esp_vfs_fat_mount_config_t mount_config{};
    mount_config.format_if_mount_failed = false;
    mount_config.max_files = CONFIG_STACKCHAN_SD_MAX_FILES;
    mount_config.allocation_unit_size = 16 * 1024;

    err = esp_vfs_fat_sdspi_mount(CONFIG_STACKCHAN_SD_MOUNT_PATH, &host, &slot_config, &mount_config, &s_sd_card);
    if (err == ESP_OK) {
        s_sd_ready = true;
        ESP_LOGI(TAG_SD, "mounted at %s, loading assets from %s/%s",
                 CONFIG_STACKCHAN_SD_MOUNT_PATH,
                 CONFIG_STACKCHAN_SD_MOUNT_PATH,
                 CONFIG_STACKCHAN_SD_ASSETS_SUBPATH);
    } else {
        ESP_LOGW(TAG_SD, "SD mount failed (%s), continuing without SD assets", esp_err_to_name(err));
        s_sd_ready = false;
    }
}

static bool read_sd_into_cache_locked(const std::string& key)
{
    if (!s_sd_ready || key.empty()) {
        return false;
    }

    return read_file_into_cache_locked(make_source_cache_key("sd", key), make_sd_path(key));
}
#endif /* CONFIG_STACKCHAN_SD_UI_ASSETS */

#if CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS
static bool s_lfs_attempted = false;
static bool s_lfs_ready = false;

static std::string make_lfs_path(std::string_view name)
{
    return make_asset_path(CONFIG_STACKCHAN_LITTLEFS_MOUNT_PATH, CONFIG_STACKCHAN_LITTLEFS_ASSETS_SUBPATH, name);
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

    return read_file_into_cache_locked(make_source_cache_key("lfs", key), make_lfs_path(key));
}
#endif /* CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS */

}  // namespace

namespace stackchan_assets {

void early_init_launcher_assets()
{
#if CONFIG_STACKCHAN_SD_UI_ASSETS
    mount_sd_once();
#endif
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

#ifdef CONFIG_STACKCHAN_SD_UI_ASSETS
    {
        std::lock_guard<std::mutex> guard(s_cache_mtx);

        mount_sd_once();

        const std::string sd_cache_key = make_source_cache_key("sd", key);
        auto it = s_fs_cache.find(sd_cache_key);
        if (it == s_fs_cache.end()) {
            read_sd_into_cache_locked(key);
            it = s_fs_cache.find(sd_cache_key);
        }

        if (it != s_fs_cache.end()) {
            *out_ptr = it->second.data();
            *out_len = it->second.size();
            if (out_src) {
                *out_src = LauncherAssetSource::SDCard;
            }
            return true;
        }
    }
#endif

#if CONFIG_STACKCHAN_LITTLEFS_UI_ASSETS
    {
        std::lock_guard<std::mutex> guard(s_cache_mtx);

        mount_lfs_once();

        const std::string lfs_cache_key = make_source_cache_key("lfs", key);
        auto it = s_fs_cache.find(lfs_cache_key);
        if (it == s_fs_cache.end()) {
            read_lfs_into_cache_locked(key);
            it = s_fs_cache.find(lfs_cache_key);
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

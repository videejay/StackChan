/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
// https://github.com/espressif/esp-idf/blob/v5.5.2/examples/system/ota/simple_ota_example/main/simple_ota_example.c

// #define CONFIG_EXAMPLE_USE_CERT_BUNDLE 1

#include "ota.h"
#include "sdkconfig.h"
#include "stackchan_asset_provider.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "string.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
#include "esp_crt_bundle.h"
#endif

static const char *TAG = "ota";

#define OTA_URL_SIZE 256

const char *server_cert_pem_start =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIC+jCCAeKgAwIBAgIUBhxIdyxfbSgLqwvNPcelYLE88Y0wDQYJKoZIhvcNAQEL\n"
    "BQAwJDEiMCAGA1UEAwwZU3RhY2tjaGFuIE9UQSBUZXN0IFNlcnZlcjAeFw0yNjAx\n"
    "MjAwMjE4NTRaFw0yNzAxMjAwMjE4NTRaMCQxIjAgBgNVBAMMGVN0YWNrY2hhbiBP\n"
    "VEEgVGVzdCBTZXJ2ZXIwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQCg\n"
    "xRAxiju6admmBoCeC9wPYLLE9tUHccYM69O637Qapw3n8zH9LURBTJ7ivCeVQC8T\n"
    "+pZGLkS4NsExS+oHnyyr+OTB3ykKxOoOb7Sk0izxy0+gDEEojhNUPYc/mNgAq4yw\n"
    "ELmq5ymFMe1nFbm++menfdsYcyFNw05J/8c0gaOM2mj+GbGrzLUXVyAZg3JNKFEQ\n"
    "SfIgI41XNAWWNozRSrtbPUSBbmuCaOoQeNrU1jt5mBsVuHk5p7wIt2jJUe13a6UE\n"
    "0N179S+1Cn0fMEceJWYBS5FBSU83L2DMJi+FXI/877NKq/gifzYccG8tg1mYabba\n"
    "lKGNdhtxx6UJv0DtobUlAgMBAAGjJDAiMCAGA1UdEQQZMBeCCWxvY2FsaG9zdIcE\n"
    "fwAAAYcEwKgUFzANBgkqhkiG9w0BAQsFAAOCAQEAoHi9RYFuB6EKVU21rWSDPf/O\n"
    "9PhDcp8+hrE5OdgowhTeZDLwy6b/uAF7Vgo/Ojk/oqrHvFJlHEH3wQFTbWjUIJ3P\n"
    "aAzHrZEYMOGRTPdELiilke6+HbMMbOGfhFqt7es8eXPwFdzraPGaodwf0W8/AYSk\n"
    "QQoW+5zbkOS5p1teQUTsnrccjfe1xDx/mz1bW1cHK69pQNGbVtWpGs5IDMesDcqL\n"
    "nFnhrKtz3LFETQH5ItSueFUEurnyqxY+4rV2kx8emGtzPSokwbGmIpVe+Bei7sSo\n"
    "fQAFvk9rhUf1vvphC+Hci1uR60yDMq1P7S9JT+rRPTqdi+RoMK3LpVx67Ie0Hg==\n"
    "-----END CERTIFICATE-----\n"
    "";

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

esp_err_t my_esp_https_ota(const esp_https_ota_config_t *ota_config, void (*on_progress)(int progress))
{
    if (ota_config == NULL || ota_config->http_config == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err                           = esp_https_ota_begin(ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        return err;
    }

    int last_progress = -1;
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }

        // --- 进度计算逻辑 ---
        int total_size = esp_https_ota_get_image_size(https_ota_handle);
        int read_size  = esp_https_ota_get_image_len_read(https_ota_handle);

        if (total_size > 0) {
            int progress = (read_size * 100) / total_size;

            if (progress != last_progress) {
                last_progress = progress;
                if (on_progress) {
                    on_progress(progress);
                }
            }
        }
    }

    if (err != ESP_OK) {
        esp_https_ota_abort(https_ota_handle);
        return err;
    }

    esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if (ota_finish_err != ESP_OK) {
        return ota_finish_err;
    }
    return ESP_OK;
}

void start_ota_update(const char *url, void (*on_progress)(int progress))
{
    ESP_LOGI(TAG, "Starting OTA example task");
#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
    esp_netif_t *netif = get_example_netif_from_desc(bind_interface_name);
    if (netif == NULL) {
        ESP_LOGE(TAG, "Can't find netif from interface description");
        abort();
    }
    struct ifreq ifr;
    esp_netif_get_netif_impl_name(netif, ifr.ifr_name);
    ESP_LOGI(TAG, "Bind interface name is %s", ifr.ifr_name);
#endif
    esp_http_client_config_t config = {
        .url = url,
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .cert_pem = (char *)server_cert_pem_start,
#endif /* CONFIG_EXAMPLE_USE_CERT_BUNDLE */
        .event_handler     = _http_event_handler,
        .keep_alive_enable = true,
#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_BIND_IF
        .if_name = &ifr,
#endif
#if CONFIG_EXAMPLE_TLS_DYN_BUF_RX_STATIC
        /* This part applies static buffer strategy for rx dynamic buffer.
         * This is to avoid frequent allocation and deallocation of dynamic buffer.
         */
        .tls_dyn_buf_strategy = HTTP_TLS_DYN_BUF_RX_STATIC,
#endif /* CONFIG_EXAMPLE_TLS_DYN_BUF_RX_STATIC */
    };

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL_FROM_STDIN
    char url_buf[OTA_URL_SIZE];
    if (strcmp(config.url, "FROM_STDIN") == 0) {
        example_configure_stdin_stdout();
        fgets(url_buf, OTA_URL_SIZE, stdin);
        int len          = strlen(url_buf);
        url_buf[len - 1] = '\0';
        config.url       = url_buf;
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong firmware upgrade image url");
        abort();
    }
#endif

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
    esp_err_t ret = my_esp_https_ota(&ota_config, on_progress);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Succeed, Rebooting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
    }
}

#if CONFIG_STACKCHAN_SD_UI_ASSETS
#define APP_STORE_SD_APPS_DIR CONFIG_STACKCHAN_SD_MOUNT_PATH "/apps"

static void sanitize_basename(const char *in, char *out, size_t out_sz)
{
    size_t j = 0;
    if (in == NULL || out_sz < 4) {
        if (out_sz) {
            strncpy(out, "app", out_sz - 1);
            out[out_sz - 1] = '\0';
        }
        return;
    }
    for (size_t i = 0; in[i] != '\0' && j + 1 < out_sz; ++i) {
        unsigned char c = (unsigned char)in[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
            out[j++] = (char)c;
        } else if (j > 0 && out[j - 1] != '_') {
            out[j++] = '_';
        }
    }
    if (j == 0) {
        strncpy(out, "app", out_sz - 1);
        out[out_sz - 1] = '\0';
    } else {
        out[j] = '\0';
    }
    if (strlen(out) > 40) {
        out[40] = '\0';
    }
}

static void basename_from_url(const char *url, char *out, size_t out_sz)
{
    const char *p = strrchr(url, '/');
    p = p ? p + 1 : url;
    const char *q = strchr(p, '?');
    size_t len = q ? (size_t)(q - p) : strlen(p);
    if (len == 0 || len >= out_sz) {
        sanitize_basename("firmware", out, out_sz);
        return;
    }
    memcpy(out, p, len);
    out[len] = '\0';
    sanitize_basename(out, out, out_sz);
}

static esp_err_t http_download_to_file(const char *url, const char *filepath, void (*on_progress)(int progress))
{
    bool use_ssl = (strncmp(url, "https://", 8) == 0);
    esp_http_client_config_t cfg = {
        .url = url,
        .event_handler = _http_event_handler,
        .keep_alive_enable = true,
        .timeout_ms = 120000,
        .transport_type = use_ssl ? HTTP_TRANSPORT_OVER_SSL : HTTP_TRANSPORT_OVER_TCP,
#ifdef CONFIG_EXAMPLE_USE_CERT_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach,
#else
        .cert_pem = use_ssl ? (char *)server_cert_pem_start : NULL,
#endif
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (client == NULL) {
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        esp_http_client_cleanup(client);
        return err;
    }

    if (esp_http_client_fetch_headers(client) < 0) {
        ESP_LOGE(TAG, "fetch_headers failed");
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int status = esp_http_client_get_status_code(client);
    if (status < 200 || status >= 300) {
        ESP_LOGE(TAG, "HTTP status %d", status);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    int64_t content_len = esp_http_client_get_content_length(client);
    FILE *f = fopen(filepath, "wb");
    if (f == NULL) {
        ESP_LOGE(TAG, "fopen %s failed", filepath);
        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    uint8_t buf[4096];
    int64_t read_total = 0;
    int last_pct = -1;

    while (1) {
        int r = esp_http_client_read(client, (char *)buf, sizeof(buf));
        if (r < 0) {
            err = ESP_FAIL;
            break;
        }
        if (r == 0) {
            err = ESP_OK;
            break;
        }
        if (fwrite(buf, 1, (size_t)r, f) != (size_t)r) {
            err = ESP_FAIL;
            break;
        }
        read_total += r;
        if (on_progress && content_len > 0) {
            int pct = (int)((read_total * 45) / content_len);
            if (pct > 45) {
                pct = 45;
            }
            if (pct != last_pct) {
                last_pct = pct;
                on_progress(pct);
            }
        }
    }

    fclose(f);
    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        unlink(filepath);
    } else if (on_progress && content_len <= 0) {
        on_progress(45);
    }

    return err;
}

static esp_err_t ota_flash_from_file(const char *filepath, void (*on_progress)(int progress))
{
    struct stat st;
    if (stat(filepath, &st) != 0 || st.st_size <= 0) {
        ESP_LOGE(TAG, "invalid firmware file %s", filepath);
        return ESP_FAIL;
    }

    const esp_partition_t *part = esp_ota_get_next_update_partition(NULL);
    if (part == NULL) {
        ESP_LOGE(TAG, "no OTA partition");
        return ESP_ERR_NOT_FOUND;
    }

    esp_ota_handle_t ota_handle = 0;
    esp_err_t err = esp_ota_begin(part, (size_t)st.st_size, &ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        return err;
    }

    FILE *f = fopen(filepath, "rb");
    if (f == NULL) {
        esp_ota_abort(ota_handle);
        return ESP_FAIL;
    }

    uint8_t buf[4096];
    size_t written = 0;
    int last_pct = -1;

    while (1) {
        size_t r = fread(buf, 1, sizeof(buf), f);
        if (r == 0) {
            if (ferror(f)) {
                err = ESP_FAIL;
            }
            break;
        }
        err = esp_ota_write(ota_handle, buf, r);
        if (err != ESP_OK) {
            break;
        }
        written += r;
        if (on_progress) {
            int pct = 45 + (int)((written * 55) / (size_t)st.st_size);
            if (pct > 99) {
                pct = 99;
            }
            if (pct != last_pct) {
                last_pct = pct;
                on_progress(pct);
            }
        }
    }

    fclose(f);

    if (err != ESP_OK) {
        esp_ota_abort(ota_handle);
        return err;
    }

    err = esp_ota_end(ota_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_ota_set_boot_partition(part);
    if (err != ESP_OK) {
        return err;
    }

    if (on_progress) {
        on_progress(100);
    }
    return ESP_OK;
}

void start_ota_update_cached_on_sd(const char *url, const char *storage_basename,
                                   void (*on_progress)(int progress))
{
    stackchan_ensure_sd_mounted();

    char slug[48];
    if (storage_basename != NULL && storage_basename[0] != '\0') {
        sanitize_basename(storage_basename, slug, sizeof(slug));
    } else {
        basename_from_url(url, slug, sizeof(slug));
    }

    if (mkdir(APP_STORE_SD_APPS_DIR, 0755) != 0) {
        struct stat st;
        if (stat(APP_STORE_SD_APPS_DIR, &st) != 0 || !S_ISDIR(st.st_mode)) {
            ESP_LOGW(TAG, "SD apps dir missing (%s) - falling back to direct HTTPS OTA", APP_STORE_SD_APPS_DIR);
            start_ota_update(url, on_progress);
            return;
        }
    }

    char filepath[192];
    int n = snprintf(filepath, sizeof(filepath), "%s/%s.bin", APP_STORE_SD_APPS_DIR, slug);
    if (n <= 0 || n >= (int)sizeof(filepath)) {
        ESP_LOGE(TAG, "firmware path too long");
        return;
    }

    ESP_LOGI(TAG, "App store: downloading to %s", filepath);
    esp_err_t err = http_download_to_file(url, filepath, on_progress);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "download failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "App store: flashing from SD cache");
    err = ota_flash_from_file(filepath, on_progress);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA from file failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "OTA from SD cache OK, rebooting...");
    esp_restart();
}
#else /* !CONFIG_STACKCHAN_SD_UI_ASSETS */

void start_ota_update_cached_on_sd(const char *url, const char *storage_basename,
                                   void (*on_progress)(int progress))
{
    (void)storage_basename;
    start_ota_update(url, on_progress);
}

#endif /* CONFIG_STACKCHAN_SD_UI_ASSETS */

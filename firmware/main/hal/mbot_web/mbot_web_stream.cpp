/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "mbot_web_stream.h"

#include <hal/avatar_camera_stream.h>

#include <ArduinoJson.h>
#include <esp_http_server.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/hal.h>

#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

namespace {

constexpr char kBoundary[] = "frame";
constexpr uint32_t kWebCaptureIntervalMs = 100;

std::mutex g_mjpeg_client_mutex;
std::atomic<bool> g_mjpeg_client_active{false};
std::atomic<bool> g_stream_abort{false};
std::atomic<bool> g_web_stream_enabled{false};

}  // namespace

void mbotWebStreamSetEnabled(bool enabled)
{
    const bool was = g_web_stream_enabled.exchange(enabled);
    if (enabled && !was) {
        GetHAL().setCameraLedActive(true);
        avatarCameraStreamAcquire();
    } else if (!enabled && was) {
        GetHAL().setCameraLedActive(false);
        avatarCameraStreamRelease();
        avatarCameraStreamClearLatestJpeg();
    }
}

bool mbotWebStreamIsEnabled()
{
    return g_web_stream_enabled.load();
}

void mbotWebAvatarTick()
{
    if (!g_web_stream_enabled.load()) {
        return;
    }
    avatarCameraStreamTick(kWebCaptureIntervalMs);
}

void mbotWebStreamAbort()
{
    g_stream_abort.store(true);
}

void mbotWebStreamHandleToggle(const char* body, ArduinoJson::JsonDocument& outDoc)
{
    ArduinoJson::JsonDocument req;
    if (body && body[0]) {
        deserializeJson(req, body);
    }

    ArduinoJson::JsonObject obj = outDoc.to<ArduinoJson::JsonObject>();
    if (req["enabled"].is<bool>()) {
        const bool en = req["enabled"].as<bool>();
        mbotWebStreamSetEnabled(en);
        obj["ok"]        = true;
        obj["message"]   = en ? "stream on" : "stream off";
        obj["streaming"] = en;
        return;
    }

    obj["ok"]        = true;
    obj["message"]   = "stream status";
    obj["streaming"] = mbotWebStreamIsEnabled();
}

esp_err_t mbotWebStreamMjpegHandler(httpd_req_t* req)
{
    if (!mbotWebStreamIsEnabled()) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "stream not enabled");
        return ESP_FAIL;
    }

    {
        std::lock_guard<std::mutex> lock(g_mjpeg_client_mutex);
        if (g_mjpeg_client_active.exchange(true)) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "stream busy");
            return ESP_FAIL;
        }
    }

    g_stream_abort.store(false);

    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Connection", "close");

    esp_err_t result = ESP_OK;

    while (!g_stream_abort.load()) {
        std::vector<uint8_t> jpeg;
        if (!avatarCameraStreamCopyLatestJpeg(jpeg)) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        char partHeader[128];
        const int headerLen = snprintf(partHeader, sizeof(partHeader),
                                       "\r\n--%s\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
                                       kBoundary, static_cast<unsigned>(jpeg.size()));
        if (headerLen <= 0 || headerLen >= static_cast<int>(sizeof(partHeader))) {
            result = ESP_FAIL;
            break;
        }

        if (httpd_resp_send_chunk(req, partHeader, headerLen) != ESP_OK) {
            result = ESP_FAIL;
            break;
        }
        if (httpd_resp_send_chunk(req, reinterpret_cast<const char*>(jpeg.data()), jpeg.size()) != ESP_OK) {
            result = ESP_FAIL;
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    httpd_resp_send_chunk(req, nullptr, 0);

    g_mjpeg_client_active.store(false);
    return result;
}

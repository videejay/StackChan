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

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

namespace {

constexpr char kStreamBoundary[] = "\r\n--frame\r\n";
constexpr char kStreamPartFmt[]  = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
/** Match Avatar WS cadence (hal_ws_avatar.cpp). */
constexpr uint32_t kWebCaptureIntervalMs = 350;
constexpr size_t kJpegSendChunkBytes     = 4096;

std::mutex g_mjpeg_client_mutex;
std::atomic<bool> g_mjpeg_client_active{false};
std::atomic<bool> g_stream_abort{false};
std::atomic<bool> g_web_stream_enabled{false};
/** Bumped on disable so an in-flight MJPEG handler exits even if re-enabled quickly. */
std::atomic<uint32_t> g_stream_session{0};

bool streamSessionActive(uint32_t session)
{
    return g_web_stream_enabled.load() && g_stream_session.load() == session;
}

esp_err_t sendMjpegFrame(httpd_req_t* req, const std::vector<uint8_t>& jpeg)
{
    char partHeader[96];
    const int headerLen =
        snprintf(partHeader, sizeof(partHeader), kStreamPartFmt, static_cast<unsigned>(jpeg.size()));
    if (headerLen <= 0 || headerLen >= static_cast<int>(sizeof(partHeader))) {
        return ESP_FAIL;
    }

    if (httpd_resp_send_chunk(req, kStreamBoundary, strlen(kStreamBoundary)) != ESP_OK) {
        return ESP_FAIL;
    }
    if (httpd_resp_send_chunk(req, partHeader, headerLen) != ESP_OK) {
        return ESP_FAIL;
    }

    size_t sent = 0;
    while (sent < jpeg.size()) {
        const size_t chunk = std::min(kJpegSendChunkBytes, jpeg.size() - sent);
        if (httpd_resp_send_chunk(req, reinterpret_cast<const char*>(jpeg.data() + sent), chunk) != ESP_OK) {
            return ESP_FAIL;
        }
        sent += chunk;
    }
    return ESP_OK;
}

void releaseStreamResources()
{
    GetHAL().setCameraLedActive(false);
    avatarCameraStreamRelease();
    avatarCameraStreamClearLatestJpeg();
}

void setStreamEnabled(bool enabled)
{
    if (!enabled) {
        ++g_stream_session;
        g_stream_abort.store(true);
    } else {
        g_stream_abort.store(false);
    }

    const bool was = g_web_stream_enabled.exchange(enabled);
    if (enabled && !was) {
        GetHAL().setCameraLedActive(true);
        avatarCameraStreamAcquire();
    } else if (!enabled && was) {
        releaseStreamResources();
    }
}

esp_err_t runMjpegHandler(httpd_req_t* req)
{
    if (!g_web_stream_enabled.load()) {
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

    const uint32_t session = g_stream_session.load();

    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
    httpd_resp_set_hdr(req, "Pragma", "no-cache");

    esp_err_t result = ESP_OK;

    while (streamSessionActive(session) && !g_stream_abort.load()) {
        std::vector<uint8_t> jpeg;
        if (!avatarCameraStreamCaptureFrame(jpeg, kWebCaptureIntervalMs)) {
            if (!streamSessionActive(session)) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }

        if (!streamSessionActive(session)) {
            break;
        }

        if (sendMjpegFrame(req, jpeg) != ESP_OK) {
            result = ESP_FAIL;
            break;
        }
    }

    httpd_resp_send_chunk(req, nullptr, 0);

    g_mjpeg_client_active.store(false);
    g_stream_abort.store(false);

    if (result != ESP_OK && g_web_stream_enabled.load()) {
        ++g_stream_session;
        g_web_stream_enabled.store(false);
        releaseStreamResources();
    }

    return result;
}

bool isStreamEnabled()
{
    return g_web_stream_enabled.load();
}

void abortStream()
{
    g_stream_abort.store(true);
}

}  // namespace

void mbotWebStreamSetEnabled(bool enabled)
{
    setStreamEnabled(enabled);
}

bool mbotWebStreamIsEnabled()
{
    return isStreamEnabled();
}

void mbotWebAvatarTick()
{
    /* HTTP MJPEG captures synchronously in mbotWebStreamMjpegHandler(). */
}

void mbotWebStreamAbort()
{
    abortStream();
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
    return runMjpegHandler(req);
}

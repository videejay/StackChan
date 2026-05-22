/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "mbot_web_server.h"

#include "mbot_web_api.h"
#include "mbot_web_stream.h"

#include <assets/stackchan_asset_provider.hpp>
#include <ArduinoJson.h>
#include <esp_http_server.h>
#include <mooncake_log.h>

#include <cstring>
#include <string>

namespace {

constexpr char TAG[] = "MbotWeb";
constexpr char kIndexAssetName[] = "index.html";
httpd_handle_t g_server = nullptr;

std::string pathOnly(const char* uri)
{
    if (!uri) {
        return {};
    }
    const char* q = strchr(uri, '?');
    if (!q) {
        return std::string(uri);
    }
    return std::string(uri, q - uri);
}

esp_err_t sendJson(httpd_req_t* req, ArduinoJson::JsonDocument& doc)
{
    std::string out;
    serializeJson(doc, out);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, out.c_str(), HTTPD_RESP_USE_STRLEN);
}

esp_err_t readBody(httpd_req_t* req, std::string& body)
{
    body.clear();
    if (req->content_len <= 0) {
        return ESP_OK;
    }
    const size_t len = static_cast<size_t>(req->content_len);
    if (len > 2048) {
        return ESP_ERR_INVALID_SIZE;
    }
    body.resize(len);
    size_t received = 0;
    while (received < len) {
        const int ret = httpd_req_recv(req, body.data() + received, len - received);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            return ESP_FAIL;
        }
        received += static_cast<size_t>(ret);
    }
    return ESP_OK;
}

esp_err_t indexHandler(httpd_req_t* req)
{
    const uint8_t* data = nullptr;
    std::size_t len     = 0;
    stackchan_assets::LauncherAssetSource src = stackchan_assets::LauncherAssetSource::Embedded;

    if (!stackchan_assets::get_launcher_ui_asset_bytes(kIndexAssetName, &data, &len, &src) || data == nullptr ||
        len == 0) {
        mclog::tagError(TAG, "{} missing (place under SD or LittleFS assets/)", kIndexAssetName);
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "index.html not found in assets");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, reinterpret_cast<const char*>(data), len);
}

esp_err_t apiGetHandler(httpd_req_t* req)
{
    ArduinoJson::JsonDocument doc;
    const std::string path = pathOnly(req->uri);
    mbotWebHandleApi("GET", path.c_str(), "", doc);
    return sendJson(req, doc);
}

esp_err_t apiPostHandler(httpd_req_t* req)
{
    std::string body;
    const esp_err_t readErr = readBody(req, body);
    if (readErr != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad body");
        return readErr;
    }

    ArduinoJson::JsonDocument doc;
    const std::string path = pathOnly(req->uri);
    mbotWebHandleApi("POST", path.c_str(), body.c_str(), doc);
    return sendJson(req, doc);
}

httpd_uri_t makeUri(const char* uri, httpd_method_t method, esp_err_t (*handler)(httpd_req_t*))
{
    httpd_uri_t u = {};
    u.uri         = uri;
    u.method      = method;
    u.handler     = handler;
    u.user_ctx    = nullptr;
    return u;
}

bool registerUri(httpd_handle_t server, const httpd_uri_t& uri)
{
    return httpd_register_uri_handler(server, &uri) == ESP_OK;
}

}  // namespace

bool mbotWebIsActive()
{
    return g_server != nullptr;
}

bool mbotWebStart(uint16_t port)
{
    if (g_server) {
        return true;
    }

    stackchan_assets::early_init_launcher_assets();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port    = port;
    config.max_uri_handlers = 24;
    config.lru_purge_enable = true;

    if (httpd_start(&g_server, &config) != ESP_OK) {
        mclog::tagError(TAG, "httpd_start failed");
        g_server = nullptr;
        return false;
    }

    const httpd_uri_t routes[] = {
        makeUri("/", HTTP_GET, indexHandler),
        makeUri("/api/ping", HTTP_GET, apiGetHandler),
        makeUri("/api/distance", HTTP_GET, apiGetHandler),
        makeUri("/api/line", HTTP_GET, apiGetHandler),
        makeUri("/api/status", HTTP_GET, apiGetHandler),
        makeUri("/api/stop", HTTP_POST, apiPostHandler),
        makeUri("/api/clear", HTTP_POST, apiPostHandler),
        makeUri("/api/motors", HTTP_POST, apiPostHandler),
        makeUri("/api/text", HTTP_POST, apiPostHandler),
        makeUri("/api/rgb", HTTP_POST, apiPostHandler),
        makeUri("/api/servo", HTTP_POST, apiPostHandler),
        makeUri("/api/tone", HTTP_POST, apiPostHandler),
        makeUri("/api/avatar/status", HTTP_GET, apiGetHandler),
        makeUri("/api/avatar/emotion", HTTP_POST, apiPostHandler),
        makeUri("/api/avatar/speech", HTTP_POST, apiPostHandler),
        makeUri("/api/avatar/speech/clear", HTTP_POST, apiPostHandler),
        makeUri("/api/avatar/head", HTTP_POST, apiPostHandler),
        makeUri("/api/avatar/head/home", HTTP_POST, apiPostHandler),
        makeUri("/api/camera/stream", HTTP_POST, apiPostHandler),
        makeUri("/api/camera/stream.mjpg", HTTP_GET, mbotWebStreamMjpegHandler),
    };

    for (const auto& route : routes) {
        if (!registerUri(g_server, route)) {
            mclog::tagError(TAG, "register {} failed", route.uri);
            mbotWebStop();
            return false;
        }
    }

    mclog::tagInfo(TAG, "HTTP server started on port {}", port);
    return true;
}

void mbotWebStop()
{
    if (!g_server) {
        return;
    }
    mbotWebStreamAbort();
    mbotWebStreamSetEnabled(false);
    httpd_stop(g_server);
    g_server = nullptr;
    mclog::tagInfo(TAG, "HTTP server stopped");
}

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <ArduinoJson.h>
#include <esp_http_server.h>

void mbotWebStreamSetEnabled(bool enabled);
bool mbotWebStreamIsEnabled();

void mbotWebStreamAbort();

/** Called from WEB.REMOTE main loop when HTTP server is active. */
void mbotWebAvatarTick();

esp_err_t mbotWebStreamMjpegHandler(httpd_req_t* req);

void mbotWebStreamHandleToggle(const char* body, ArduinoJson::JsonDocument& outDoc);

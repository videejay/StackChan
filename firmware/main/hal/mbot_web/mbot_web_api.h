/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <ArduinoJson.h>

void mbotWebHandleApi(const char* method, const char* path, const char* body, ArduinoJson::JsonDocument& outDoc);

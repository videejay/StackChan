/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "mbot_client.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <hal/board/hal_bridge.h>

namespace {
constexpr char TAG[]          = "MbotClient";
constexpr uint8_t kAddress    = 0x10;
constexpr uint8_t kStartByte  = 0xAA;
constexpr uint8_t kMaxPayload = 24;
constexpr int kTimeoutMs      = 100;
}  // namespace

MbotClient& MbotClient::GetInstance()
{
    static MbotClient client;
    return client;
}

bool MbotClient::init()
{
    if (device_) {
        ready_ = ready_ || ping();
        return ready_;
    }

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = kAddress,
        .scl_speed_hz    = 100 * 1000,
        .scl_wait_us     = 0,
        .flags           = {.disable_ack_check = 0},
    };

    auto bus = hal_bridge::board_get_i2c_bus();
    if (i2c_master_bus_add_device(bus, &cfg, &device_) != ESP_OK) {
        ESP_LOGW(TAG, "Failed to add mBot I2C device");
        return false;
    }
    while(!ready_){
        ready_ = ping();
        ESP_LOGI(TAG, "mBot client %s", ready_ ? "ready" : "not responding");
        lv_sleep_ms(5000);
    }
    return ready_;
}

bool MbotClient::isReady() const
{
    return ready_;
}

bool MbotClient::ping()
{
    ESP_LOGI(TAG, "mBot PING");
    return transact(0x06, nullptr, 0, nullptr, 0);
}

bool MbotClient::setMotors(int8_t left, int8_t right)
{
    ESP_LOGI(TAG, "mBot set Motors Left: %d - Right: %d", left, right);
    uint8_t payload[2] = {static_cast<uint8_t>(left), static_cast<uint8_t>(right)};
    return transact(0x01, payload, sizeof(payload), nullptr, 0);
}

bool MbotClient::stopMotors()
{
    ESP_LOGW(TAG, "mBot stopping motors");
    return transact(0x08, nullptr, 0, nullptr, 0);
}

bool MbotClient::getDistanceCm(uint16_t& distanceCm)
{
    uint8_t payload[2] = {};
    ESP_LOGI(TAG, "mBot distance requested");
    if (!transact(0x02, nullptr, 0, payload, sizeof(payload))) {
        return false;
    }
    distanceCm = static_cast<uint16_t>((payload[0] << 8) | payload[1]);
    ESP_LOGI(TAG, "mBot distance received: %dcm", distanceCm);
    return true;
}

bool MbotClient::getLineFollower(uint8_t& state)
{
    uint8_t payload[1] = {};
    ESP_LOGI(TAG, "mBot line follower requested");
    if (!transact(0x0B, nullptr, 0, payload, sizeof(payload))) {
        return false;
    }
    state = payload[0];
    ESP_LOGI(TAG, "mBot line follower response %d", state);
    return true;
}

bool MbotClient::displayText(const char* text)
{
    if (!text) {
        return false;
    }
    const auto len = static_cast<uint8_t>(std::min<size_t>(std::strlen(text), kMaxPayload));
    ESP_LOGI(TAG, "mBot text send %s with len: %d", text, len);
    return transact(0x03, reinterpret_cast<const uint8_t*>(text), len, nullptr, 0);
}

bool MbotClient::setRgbLed(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t payload[3] = {r, g, b};
    ESP_LOGI(TAG, "mBot RGB LED command send R: %d G: %d B: %d", r,g,b);
    return transact(0x09, payload, sizeof(payload), nullptr, 0);
}

bool MbotClient::playTone(uint16_t frequencyHz, uint16_t durationMs)
{
    uint8_t payload[4] = {
        static_cast<uint8_t>(frequencyHz >> 8),
        static_cast<uint8_t>(frequencyHz & 0xFF),
        static_cast<uint8_t>(durationMs >> 8),
        static_cast<uint8_t>(durationMs & 0xFF),
    };
    ESP_LOGI(TAG, "mBot play tone Freq: %d Duration: %d", frequencyHz, durationMs);
    return transact(0x0A, payload, sizeof(payload), nullptr, 0);
}

bool MbotClient::displayBitmap(uint8_t width, const uint8_t* bitmap, uint8_t bitmapLen)
{
    if (bitmapLen + 1 > kMaxPayload) {
        return false;
    }
    std::array<uint8_t, kMaxPayload> payload = {};
    payload[0]                               = width;
    std::memcpy(payload.data() + 1, bitmap, bitmapLen);
    ESP_LOGI(TAG, "mBot send bitmap - width: %d - len: %d ",width, bitmapLen);
    return transact(0x07, payload.data(), bitmapLen + 1, nullptr, 0);
}

bool MbotClient::clearDisplay()
{
    ESP_LOGI(TAG, "mBot clear display");
    return transact(0x04, nullptr, 0, nullptr, 0);
}

bool MbotClient::transact(uint8_t cmd, const uint8_t* payload, uint8_t payloadLen, uint8_t* responsePayload,
                          uint8_t expectedResponseLen)
{
    if (!device_ || payloadLen > kMaxPayload) {
        return false;
    }

    std::array<uint8_t, 32> request = {};
    request[0]                      = kStartByte;
    request[1]                      = static_cast<uint8_t>(payloadLen + 2);
    request[2]                      = cmd;
    if (payloadLen > 0 && payload) {
        std::memcpy(request.data() + 3, payload, payloadLen);
    }
    request[3 + payloadLen] = crc8(request.data() + 1, payloadLen + 2);

    if (i2c_master_transmit(device_, request.data(), payloadLen + 4, kTimeoutMs) != ESP_OK) {
        ready_ = false;
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(2));

    const uint8_t expectedFrameLen = expectedResponseLen + 4;
    std::array<uint8_t, 32> response = {};
    if (i2c_master_receive(device_, response.data(), expectedFrameLen, kTimeoutMs) != ESP_OK) {
        ready_ = false;
        return false;
    }

    if (response[0] != kStartByte || response[1] != expectedResponseLen + 2 || response[2] != cmd) {
        ready_ = false;
        return false;
    }

    if (crc8(response.data() + 1, expectedResponseLen + 2) != response[3 + expectedResponseLen]) {
        ready_ = false;
        return false;
    }

    if (responsePayload && expectedResponseLen > 0) {
        std::memcpy(responsePayload, response.data() + 3, expectedResponseLen);
    }

    ready_ = true;
    return true;
}

uint8_t MbotClient::crc8(const uint8_t* data, uint8_t len)
{
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07) : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

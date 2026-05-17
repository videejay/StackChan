/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <cstdint>
#include <driver/i2c_master.h>

class MbotClient {
public:
    static MbotClient& GetInstance();

    bool init();
    bool isReady() const;

    bool ping();
    bool setMotors(int8_t left, int8_t right);
    bool stopMotors();
    bool getDistanceCm(uint16_t& distanceCm);
    bool getLineFollower(uint8_t& state);
    bool displayText(const char* text);
    bool setRgbLed(uint8_t r, uint8_t g, uint8_t b);
    bool playTone(uint16_t frequencyHz, uint16_t durationMs);
    bool displayBitmap(uint8_t width, const uint8_t* bitmap, uint8_t bitmapLen);
    bool clearDisplay();

private:
    MbotClient() = default;

    bool transact(uint8_t cmd, const uint8_t* payload, uint8_t payloadLen, uint8_t* responsePayload,
                  uint8_t expectedResponseLen);
    static uint8_t crc8(const uint8_t* data, uint8_t len);

    i2c_master_dev_handle_t device_ = nullptr;
    bool ready_                      = false;
};

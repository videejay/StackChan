/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <array>
#include <cstdint>
#include <driver/i2c_master.h>
#include <esp_err.h>
#include <string>

class MbotClient {
public:
    enum class LinkState {
        Idle,
        Probing,
        ProbeNack,
        ProbeTimeout,
        AddDeviceFailed,
        Handshaking,
        Ready,
        ProtocolError,
    };

    struct LinkStatus {
        LinkState state = LinkState::Idle;
        uint32_t attempt = 0;
        esp_err_t lastEspErr = ESP_OK;
        uint8_t lastCmd      = 0;
        uint8_t lastBadByte  = 0;
        std::array<uint8_t, 32> lastTxFrame = {};
        uint8_t lastTxLen                   = 0;
        std::array<uint8_t, 32> lastRxFrame = {};
        uint8_t lastRxLen                   = 0;
        const char* lastReason              = "";
    };

    static MbotClient& GetInstance();

    /** One-shot: set verbose logging for this tag, then tryConnectOnce(). */
    bool init();

    /** Single non-blocking connect attempt (probe → add device → ping). Safe to call every frame. */
    bool tryConnectOnce();

    bool isReady() const;

    LinkStatus snapshot() const
    {
        return link_;
    }

    /** Reset attempt counter and idle state when opening a connection UI. */
    void resetConnectionDiagnostics();

    /** Same scan as board boot I2C map; for on-device debug. */
    static std::string rescanBus();

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

    void ensureVerboseLog();

    bool transact(uint8_t cmd, const uint8_t* payload, uint8_t payloadLen, uint8_t* responsePayload,
                  uint8_t expectedResponseLen);
    static uint8_t crc8(const uint8_t* data, uint8_t len);

    i2c_master_dev_handle_t device_ = nullptr;
    bool ready_                      = false;
    LinkStatus link_{};
};

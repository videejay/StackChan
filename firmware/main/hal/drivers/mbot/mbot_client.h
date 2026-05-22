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
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
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

    /** Single connect attempt (probe → add device → optional ping). Thread-safe. */
    bool tryConnectOnce();

    /**
     * True when the link is up. Drops idle sessions (no traffic for 10 s) and retries connect
     * 10 s after a loss. Thread-safe.
     */
    bool maintainLink();

    bool isReady() const;

    LinkStatus snapshot() const
    {
        return link_;
    }

    /** Reset attempt counter and idle state when opening a connection UI. */
    void resetConnectionDiagnostics();

    /** Same scan as board boot I2C map; for on-device debug. */
    static std::string rescanBus();

    /** Send PING only if no other mBot I2C traffic succeeded within the last 5 s. */
    bool ping();

    /**
     * Background keepalive: ping first (when idle), then enforce link state.
     * Call from the main loop at highest priority — before LVGL or camera work.
     */
    bool serviceKeepAlive();
    bool setMotors(int8_t left, int8_t right);
    bool stopMotors();
    bool getDistanceCm(uint16_t& distanceCm);
    bool getLineFollower(uint8_t& state);
    bool displayText(const char* text);
    bool setRgbLed(uint8_t r, uint8_t g, uint8_t b);
    bool playTone(uint16_t frequencyHz, uint16_t durationMs);
    bool setServoAngle(uint8_t angle);
    bool displayBitmap(uint8_t width, const uint8_t* bitmap, uint8_t bitmapLen);
    bool clearDisplay();

private:
    struct BusLockGuard {
        explicit BusLockGuard(MbotClient& client) : client_(client), locked_(client_.lockBus())
        {
        }

        ~BusLockGuard()
        {
            if (locked_) {
                client_.unlockBus();
            }
        }

        bool locked() const
        {
            return locked_;
        }

        BusLockGuard(const BusLockGuard&)            = delete;
        BusLockGuard& operator=(const BusLockGuard&) = delete;

        MbotClient& client_;
        bool locked_;
    };

    MbotClient();

    void ensureVerboseLog();
    bool lockBus();
    void unlockBus();
    bool hasRecentTraffic() const;
    void checkIdleTimeoutLocked();
    void dropDeviceLocked();
    bool tryConnectImplLocked();
    bool ensureLinkLocked();
    bool pingImpl();
    bool transactImpl(uint8_t cmd, const uint8_t* payload, uint8_t payloadLen, uint8_t* responsePayload,
                      uint8_t expectedResponseLen);
    void markLinkDown(const char* reason, LinkState state = LinkState::ProtocolError);
    void noteDisconnectedLocked();
    static uint8_t crc8(const uint8_t* data, uint8_t len);

    mutable SemaphoreHandle_t bus_mutex_;
    i2c_master_dev_handle_t device_ = nullptr;
    bool ready_                      = false;
    uint32_t last_communication_ms_  = 0;
    uint32_t link_down_since_ms_     = 0;
    uint32_t next_reconnect_attempt_ms_ = 0;
    LinkStatus link_{};
};

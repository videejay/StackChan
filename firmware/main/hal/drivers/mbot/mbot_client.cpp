/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "mbot_client.h"

#include <algorithm>
#include <array>
#include <cstdio>
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
constexpr int kProbeTimeoutMs = 100;
constexpr int kInterFrameDelayMs = 2;

void hexDumpToBuffer(const uint8_t* data, uint8_t len, char* out, size_t outSz)
{
    if (!data || len == 0 || outSz == 0) {
        if (out && outSz) {
            out[0] = '\0';
        }
        return;
    }
    size_t pos = 0;
    for (uint8_t i = 0; i < len && pos + 4 < outSz; ++i) {
        int n = snprintf(out + pos, outSz - pos, "%02X ", data[i]);
        if (n <= 0) {
            break;
        }
        pos += static_cast<size_t>(n);
    }
    if (pos > 0 && out[pos - 1] == ' ') {
        out[pos - 1] = '\0';
    }
}
}  // namespace

MbotClient& MbotClient::GetInstance()
{
    static MbotClient client;
    return client;
}

void MbotClient::ensureVerboseLog()
{
    static bool done = false;
    if (!done) {
        esp_log_level_set(TAG, ESP_LOG_VERBOSE);
        done = true;
    }
}

bool MbotClient::init()
{
    ensureVerboseLog();
    return tryConnectOnce();
}

void MbotClient::resetConnectionDiagnostics()
{
    link_.attempt     = 0;
    link_.state       = LinkState::Idle;
    link_.lastEspErr  = ESP_OK;
    link_.lastCmd     = 0;
    link_.lastBadByte = 0;
    link_.lastTxLen   = 0;
    link_.lastRxLen   = 0;
    link_.lastReason  = "";
    link_.lastTxFrame = {};
    link_.lastRxFrame = {};
}

bool MbotClient::tryConnectOnce()
{
    ensureVerboseLog();

    if (ready_) {
        link_.state = LinkState::Ready;
        return true;
    }

    link_.attempt++;

    if (device_) {
        link_.state      = LinkState::Handshaking;
        link_.lastReason = "";
        ESP_LOGI(TAG, "Handshaking: device present, attempt=%u", static_cast<unsigned>(link_.attempt));
        if (ping()) {
            ready_           = true;
            link_.state      = LinkState::Ready;
            link_.lastEspErr = ESP_OK;
            return true;
        }
        ready_ = false;
        return false;
    }

    link_.state      = LinkState::Probing;
    link_.lastReason = "";

    auto bus = hal_bridge::board_get_i2c_bus();
    if (!bus) {
        link_.state      = LinkState::ProbeTimeout;
        link_.lastEspErr = ESP_FAIL;
        link_.lastReason = "no_i2c_bus";
        ESP_LOGW(TAG, "board_get_i2c_bus returned null");
        return false;
    }

    ESP_LOGI(TAG, "Probing 0x%02X attempt=%u", kAddress, static_cast<unsigned>(link_.attempt));
    esp_err_t probe = i2c_master_probe(bus, kAddress, pdMS_TO_TICKS(kProbeTimeoutMs));
    link_.lastEspErr = probe;
    if (probe != ESP_OK) {
        if (probe == ESP_ERR_TIMEOUT) {
            link_.state = LinkState::ProbeTimeout;
        } else {
            link_.state = LinkState::ProbeNack;
        }
        link_.lastReason = "i2c_probe_failed";
        ESP_LOGW(TAG, "probe 0x%02X failed: %s", kAddress, esp_err_to_name(probe));
        return false;
    }

    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = kAddress,
        .scl_speed_hz    = 100 * 1000,
        .scl_wait_us     = 0,
        .flags           = {.disable_ack_check = 0},
    };

    esp_err_t add = i2c_master_bus_add_device(bus, &cfg, &device_);
    link_.lastEspErr = add;
    if (add != ESP_OK) {
        link_.state      = LinkState::AddDeviceFailed;
        link_.lastReason = "add_device_failed";
        device_          = nullptr;
        ESP_LOGW(TAG, "i2c_master_bus_add_device failed: %s", esp_err_to_name(add));
        return false;
    }

    ESP_LOGI(TAG, "Slave acknowledged at 0x%02X, added device, sending PING", kAddress);
    link_.state = LinkState::Handshaking;
    if (ping()) {
        ready_           = true;
        link_.state      = LinkState::Ready;
        link_.lastEspErr = ESP_OK;
        return true;
    }

    ready_ = false;
    return false;
}

std::string MbotClient::rescanBus()
{
    auto bus = hal_bridge::board_get_i2c_bus();
    std::string out;
    out.reserve(2200);
    out += "     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n";
    if (!bus) {
        out += "(no I2C bus)\n";
        return out;
    }
    for (int i = 0; i < 128; i += 16) {
        char line[8];
        snprintf(line, sizeof(line), "%02x: ", i);
        out += line;
        for (int j = 0; j < 16; j++) {
            uint8_t address = static_cast<uint8_t>(i + j);
            esp_err_t ret = i2c_master_probe(bus, address, pdMS_TO_TICKS(200));
            if (ret == ESP_OK) {
                char buf[8];
                snprintf(buf, sizeof(buf), "%02x ", address);
                out += buf;
            } else if (ret == ESP_ERR_TIMEOUT) {
                out += "UU ";
            } else {
                out += "-- ";
            }
        }
        out += "\n";
    }
    return out;
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
    ESP_LOGI(TAG, "mBot RGB LED command send R: %d G: %d B: %d", r, g, b);
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
    ESP_LOGI(TAG, "mBot send bitmap - width: %d - len: %d ", width, bitmapLen);
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
    link_.lastCmd = cmd;

    if (!device_ || payloadLen > kMaxPayload) {
        ready_           = false;
        link_.state      = LinkState::ProtocolError;
        link_.lastReason = !device_ ? "no_device" : "payload_too_long";
        ESP_LOGW(TAG, "transact aborted: %s cmd=0x%02X", link_.lastReason, cmd);
        return false;
    }

    std::array<uint8_t, 32> request = {};
    request[0]                      = kStartByte;
    request[1]                      = static_cast<uint8_t>(payloadLen + 2);
    request[2]                      = cmd;
    if (payloadLen > 0 && payload) {
        std::memcpy(request.data() + 3, payload, payloadLen);
    }
    const uint8_t txTotal           = static_cast<uint8_t>(payloadLen + 4);
    request[3 + payloadLen]         = crc8(request.data() + 1, payloadLen + 2);

    std::memcpy(link_.lastTxFrame.data(), request.data(), txTotal);
    link_.lastTxLen = txTotal;

    char hexTx[128];
    hexDumpToBuffer(link_.lastTxFrame.data(), link_.lastTxLen, hexTx, sizeof(hexTx));
    ESP_LOGI(TAG, "TX cmd=0x%02X payloadLen=%u (%u B frame): %s", cmd, payloadLen, txTotal, hexTx);

    esp_err_t txErr = i2c_master_transmit(device_, request.data(), txTotal, kTimeoutMs);
    if (txErr != ESP_OK) {
        ready_           = false;
        link_.state      = LinkState::ProtocolError;
        link_.lastEspErr = txErr;
        link_.lastReason = "tx_failed";
        ESP_LOGW(TAG, "TX failed cmd=0x%02X esp_err=%s", cmd, esp_err_to_name(txErr));
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(kInterFrameDelayMs));

    const uint8_t expectedFrameLen = expectedResponseLen + 4;
    std::array<uint8_t, 32> response = {};

    esp_err_t rxErr = i2c_master_receive(device_, response.data(), expectedFrameLen, kTimeoutMs);
    if (rxErr != ESP_OK) {
        ready_           = false;
        link_.state      = LinkState::ProtocolError;
        link_.lastEspErr = rxErr;
        link_.lastReason = "rx_failed";
        link_.lastRxLen   = 0;
        ESP_LOGW(TAG, "RX failed cmd=0x%02X esp_err=%s (expected %u bytes)", cmd, esp_err_to_name(rxErr),
                 static_cast<unsigned>(expectedFrameLen));
        return false;
    }

    std::memcpy(link_.lastRxFrame.data(), response.data(), expectedFrameLen);
    link_.lastRxLen = expectedFrameLen;

    char hexRx[128];
    hexDumpToBuffer(link_.lastRxFrame.data(), link_.lastRxLen, hexRx, sizeof(hexRx));
    ESP_LOGV(TAG, "RX raw: %s", hexRx);

    if (response[0] != kStartByte) {
        ready_            = false;
        link_.state       = LinkState::ProtocolError;
        link_.lastReason  = "bad_start";
        link_.lastBadByte = response[0];
        ESP_LOGW(TAG, "bad START 0x%02X (want 0x%02X) cmd=0x%02X rx: %s", response[0], kStartByte, cmd, hexRx);
        return false;
    }

    const uint8_t wantLenField = static_cast<uint8_t>(expectedResponseLen + 2);
    if (response[1] != wantLenField) {
        ready_           = false;
        link_.state      = LinkState::ProtocolError;
        link_.lastReason = "bad_len";
        ESP_LOGW(TAG, "bad LEN want %u got %u cmd=0x%02X rx: %s", wantLenField, response[1], cmd, hexRx);
        return false;
    }

    if (response[2] != cmd) {
        ready_           = false;
        link_.state      = LinkState::ProtocolError;
        link_.lastReason = "bad_cmd";
        ESP_LOGW(TAG, "bad CMD echo want 0x%02X got 0x%02X rx: %s", cmd, response[2], hexRx);
        return false;
    }

    const uint8_t crcRx  = response[3 + expectedResponseLen];
    const uint8_t crcExp = crc8(response.data() + 1, expectedResponseLen + 2);
    if (crcExp != crcRx) {
        ready_           = false;
        link_.state      = LinkState::ProtocolError;
        link_.lastReason = "bad_crc";
        ESP_LOGW(TAG, "bad CRC want 0x%02X got 0x%02X cmd=0x%02X rx: %s", crcExp, crcRx, cmd, hexRx);
        return false;
    }

    if (responsePayload && expectedResponseLen > 0) {
        std::memcpy(responsePayload, response.data() + 3, expectedResponseLen);
    }

    ready_           = true;
    link_.state      = LinkState::Ready;
    link_.lastEspErr = ESP_OK;
    link_.lastReason = "";
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

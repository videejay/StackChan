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
#include <string>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/task.h>
#include <hal/board/config.h>
#include <hal/board/hal_bridge.h>

namespace {
constexpr char TAG[]          = "MbotClient";
constexpr uint8_t kAddress    = 0x10;
constexpr uint8_t kStartByte  = 0xAA;
constexpr uint8_t kMaxPayload = 24;
constexpr int kTimeoutMs      = 100;
constexpr int kProbeTimeoutMs = 100;
/** Full 128-address UI scan; keep moderate so long-press completes in a few seconds. */
constexpr int kRescanProbeTimeoutMs = 45;
constexpr int kInterFrameDelayMs    = 5;
/** mBot builds the response in loop(); poll until CMD echo matches. */
constexpr int kRxRetryDelayMs       = 5;
constexpr int kRxMaxAttempts        = 10;
constexpr int kReadCommandDelayMs   = 12;
/** Skip PING when any command succeeded more recently than this. */
constexpr uint32_t kPingIdleThresholdMs = 5000;
/** Drop session and require reconnect after this long without a successful transaction. */
constexpr uint32_t kLinkLostReconnectMs = 10000;
/** Minimum spacing between reconnect attempts after the initial 10 s wait. */
constexpr uint32_t kReconnectAttemptIntervalMs = 1000;
constexpr TickType_t kBusLockTimeoutTicks = pdMS_TO_TICKS(500);

uint32_t nowMs()
{
    return static_cast<uint32_t>(esp_timer_get_time() / 1000ULL);
}

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

/** True when RX frame is from a previous command (mBot has not updated the buffer yet). */
bool isStaleResponse(const uint8_t* response, uint8_t cmd)
{
    return response[0] == kStartByte && response[2] != cmd;
}
}  // namespace

MbotClient::MbotClient()
{
    bus_mutex_ = xSemaphoreCreateMutex();
}

MbotClient& MbotClient::GetInstance()
{
    static MbotClient client;
    return client;
}

bool MbotClient::lockBus()
{
    if (!bus_mutex_) {
        return false;
    }
    if (xSemaphoreTake(bus_mutex_, kBusLockTimeoutTicks) != pdTRUE) {
        ESP_LOGW(TAG, "I2C bus lock timeout");
        return false;
    }
    return true;
}

void MbotClient::unlockBus()
{
    if (bus_mutex_) {
        xSemaphoreGive(bus_mutex_);
    }
}

bool MbotClient::hasRecentTraffic() const
{
    if (last_communication_ms_ == 0) {
        return false;
    }
    return (nowMs() - last_communication_ms_) < kPingIdleThresholdMs;
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
    BusLockGuard guard(*this);
    link_.attempt          = 0;
    link_.state            = LinkState::Idle;
    link_.lastEspErr       = ESP_OK;
    link_.lastCmd          = 0;
    link_.lastBadByte      = 0;
    link_.lastTxLen        = 0;
    link_.lastRxLen        = 0;
    link_.lastReason       = "";
    link_.lastTxFrame      = {};
    link_.lastRxFrame      = {};
    last_communication_ms_ = 0;
    link_down_since_ms_    = 0;
    next_reconnect_attempt_ms_ = 0;
}

void MbotClient::dropDeviceLocked()
{
    if (device_) {
        i2c_master_bus_rm_device(device_);
        device_ = nullptr;
    }
    ready_ = false;
}

void MbotClient::checkIdleTimeoutLocked()
{
    if (last_communication_ms_ == 0) {
        return;
    }
    const uint32_t now = nowMs();
    if ((now - last_communication_ms_) < kLinkLostReconnectMs) {
        return;
    }
    ESP_LOGW(TAG, "mBot link idle %u ms, dropping session", static_cast<unsigned>(now - last_communication_ms_));
    dropDeviceLocked();
    if (link_down_since_ms_ == 0) {
        link_down_since_ms_ = now;
    }
}

bool MbotClient::tryConnectImplLocked()
{
    ensureVerboseLog();

    if (ready_) {
        link_.state = LinkState::Ready;
        return true;
    }

    const uint32_t now = nowMs();
    if (last_communication_ms_ != 0 && link_down_since_ms_ != 0) {
        if ((now - link_down_since_ms_) < kLinkLostReconnectMs) {
            return false;
        }
        if (now < next_reconnect_attempt_ms_) {
            return false;
        }
        next_reconnect_attempt_ms_ = now + kReconnectAttemptIntervalMs;
    }

    link_.attempt++;

    if (device_) {
        if (hasRecentTraffic()) {
            ready_               = true;
            link_.state          = LinkState::Ready;
            link_.lastEspErr     = ESP_OK;
            link_.lastReason     = "";
            link_down_since_ms_  = 0;
            return true;
        }

        link_.state      = LinkState::Handshaking;
        link_.lastReason = "";
        ESP_LOGI(TAG, "Handshaking: device present, attempt=%u", static_cast<unsigned>(link_.attempt));
        if (pingImpl()) {
            ready_              = true;
            link_.state         = LinkState::Ready;
            link_.lastEspErr    = ESP_OK;
            link_down_since_ms_ = 0;
            return true;
        }
        ready_ = false;
        return false;
    }

    link_.state      = LinkState::Probing;
    link_.lastReason = "";

    auto bus = hal_bridge::board_get_mbot_i2c_bus();
    if (!bus) {
        link_.state      = LinkState::ProbeTimeout;
        link_.lastEspErr = ESP_FAIL;
        link_.lastReason = "no_mbot_i2c_bus";
        ESP_LOGW(TAG, "board_get_mbot_i2c_bus returned null (Port A G2/G1)");
        return false;
    }

    ESP_LOGI(TAG, "Probing 0x%02X on Port A (G2/G1) attempt=%u", kAddress, static_cast<unsigned>(link_.attempt));
    esp_err_t probe  = i2c_master_probe(bus, kAddress, pdMS_TO_TICKS(kProbeTimeoutMs));
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
        .scl_speed_hz    = MBOT_I2C_FREQ_HZ,
        .scl_wait_us     = 0,
        .flags           = {.disable_ack_check = 0},
    };

    esp_err_t add    = i2c_master_bus_add_device(bus, &cfg, &device_);
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
    if (pingImpl()) {
        ready_              = true;
        link_.state         = LinkState::Ready;
        link_.lastEspErr    = ESP_OK;
        link_down_since_ms_ = 0;
        return true;
    }

    ready_ = false;
    return false;
}

bool MbotClient::ensureLinkLocked()
{
    checkIdleTimeoutLocked();
    if (ready_) {
        return true;
    }
    return tryConnectImplLocked();
}

void MbotClient::noteDisconnectedLocked()
{
    ready_ = false;
    if (link_down_since_ms_ == 0) {
        link_down_since_ms_ = nowMs();
    }
}

void MbotClient::markLinkDown(const char* reason, LinkState state)
{
    noteDisconnectedLocked();
    link_.state      = state;
    link_.lastReason = reason;
}

bool MbotClient::tryConnectOnce()
{
    BusLockGuard guard(*this);
    if (!guard.locked()) {
        return ready_;
    }
    checkIdleTimeoutLocked();
    return tryConnectImplLocked();
}

bool MbotClient::maintainLink()
{
    BusLockGuard guard(*this);
    if (!guard.locked()) {
        return ready_;
    }
    return ensureLinkLocked();
}

std::string MbotClient::rescanBus()
{
    BusLockGuard guard(MbotClient::GetInstance());

    auto bus = hal_bridge::board_get_mbot_i2c_bus();
    std::string out;
    out.reserve(384);

    out += "Port A I2C\n";
    out += "SDA=2 SCL=1\n\n";

    if (!bus) {
        out += "No I2C bus";
        return out;
    }

    constexpr size_t kMaxList = 32;
    constexpr size_t kMaxShow = 10;
    uint8_t found[kMaxList];
    size_t n_found    = 0;
    unsigned timeouts = 0;

    for (int i = 0; i < 128; ++i) {
        const uint8_t addr = static_cast<uint8_t>(i);
        esp_err_t ret      = i2c_master_probe(bus, addr, pdMS_TO_TICKS(kRescanProbeTimeoutMs));
        if (ret == ESP_OK) {
            if (n_found < kMaxList) {
                found[n_found++] = addr;
            }
        } else if (ret == ESP_ERR_TIMEOUT) {
            ++timeouts;
        }
    }

    if (n_found == 0) {
        out += "No devices";
        if (timeouts) {
            out += "\n";
            out += std::to_string(timeouts);
            out += " timeouts";
        }
        return out;
    }

    out += "Found:\n";
    const size_t n_show = std::min(n_found, kMaxShow);
    for (size_t i = 0; i < n_show; ++i) {
        char line[20];
        if (found[i] == kAddress) {
            snprintf(line, sizeof(line), "0x%02X mBot\n", found[i]);
        } else {
            snprintf(line, sizeof(line), "0x%02X\n", found[i]);
        }
        out += line;
    }
    if (n_found > n_show) {
        out += '+';
        out += std::to_string(static_cast<unsigned>(n_found - n_show));
        out += " more\n";
    }
    if (timeouts) {
        out += "\n(";
        out += std::to_string(timeouts);
        out += " bus TO)";
    }
    return out;
}

bool MbotClient::isReady() const
{
    return ready_;
}

bool MbotClient::pingImpl()
{
    if (hasRecentTraffic()) {
        ESP_LOGD(TAG, "mBot PING skipped (traffic within %u ms)", static_cast<unsigned>(kPingIdleThresholdMs));
        return true;
    }
    ESP_LOGI(TAG, "mBot PING");
    return transactImpl(0x06, nullptr, 0, nullptr, 0);
}

bool MbotClient::ping()
{
    BusLockGuard guard(*this);
    if (!guard.locked() || !ensureLinkLocked()) {
        return false;
    }
    return pingImpl();
}

bool MbotClient::setMotors(int8_t left, int8_t right)
{
    BusLockGuard guard(*this);
    if (!guard.locked() || !ensureLinkLocked()) {
        return false;
    }
    ESP_LOGI(TAG, "mBot set Motors Left: %d - Right: %d", left, right);
    uint8_t payload[2] = {static_cast<uint8_t>(left), static_cast<uint8_t>(right)};
    return transactImpl(0x01, payload, sizeof(payload), nullptr, 0);
}

bool MbotClient::stopMotors()
{
    BusLockGuard guard(*this);
    if (!guard.locked() || !ensureLinkLocked()) {
        return false;
    }
    ESP_LOGW(TAG, "mBot stopping motors");
    return transactImpl(0x08, nullptr, 0, nullptr, 0);
}

bool MbotClient::getDistanceCm(uint16_t& distanceCm)
{
    BusLockGuard guard(*this);
    if (!guard.locked() || !ensureLinkLocked()) {
        return false;
    }
    uint8_t payload[2] = {};
    ESP_LOGI(TAG, "mBot distance requested");
    if (!transactImpl(0x02, nullptr, 0, payload, sizeof(payload))) {
        return false;
    }
    distanceCm = static_cast<uint16_t>((payload[0] << 8) | payload[1]);
    ESP_LOGI(TAG, "mBot distance received: %dcm", distanceCm);
    return true;
}

bool MbotClient::getLineFollower(uint8_t& state)
{
    BusLockGuard guard(*this);
    if (!guard.locked() || !ensureLinkLocked()) {
        return false;
    }
    uint8_t payload[1] = {};
    ESP_LOGI(TAG, "mBot line follower requested");
    if (!transactImpl(0x0B, nullptr, 0, payload, sizeof(payload))) {
        return false;
    }
    state = payload[0];
    ESP_LOGI(TAG, "mBot line follower response %d", state);
    return true;
}

bool MbotClient::displayText(const char* text)
{
    BusLockGuard guard(*this);
    if (!guard.locked() || !ensureLinkLocked()) {
        return false;
    }
    if (!text) {
        return false;
    }
    const auto len = static_cast<uint8_t>(std::min<size_t>(std::strlen(text), kMaxPayload));
    ESP_LOGI(TAG, "mBot text send %s with len: %d", text, len);
    return transactImpl(0x03, reinterpret_cast<const uint8_t*>(text), len, nullptr, 0);
}

bool MbotClient::setRgbLed(uint8_t r, uint8_t g, uint8_t b)
{
    BusLockGuard guard(*this);
    if (!guard.locked() || !ensureLinkLocked()) {
        return false;
    }
    uint8_t payload[3] = {r, g, b};
    ESP_LOGI(TAG, "mBot RGB LED command send R: %d G: %d B: %d", r, g, b);
    return transactImpl(0x09, payload, sizeof(payload), nullptr, 0);
}

bool MbotClient::playTone(uint16_t frequencyHz, uint16_t durationMs)
{
    BusLockGuard guard(*this);
    if (!guard.locked() || !ensureLinkLocked()) {
        return false;
    }
    uint8_t payload[4] = {
        static_cast<uint8_t>(frequencyHz >> 8),
        static_cast<uint8_t>(frequencyHz & 0xFF),
        static_cast<uint8_t>(durationMs >> 8),
        static_cast<uint8_t>(durationMs & 0xFF),
    };
    ESP_LOGI(TAG, "mBot play tone Freq: %d Duration: %d", frequencyHz, durationMs);
    return transactImpl(0x0A, payload, sizeof(payload), nullptr, 0);
}

bool MbotClient::displayBitmap(uint8_t width, const uint8_t* bitmap, uint8_t bitmapLen)
{
    BusLockGuard guard(*this);
    if (!guard.locked() || !ensureLinkLocked()) {
        return false;
    }
    if (bitmapLen + 1 > kMaxPayload) {
        return false;
    }
    std::array<uint8_t, kMaxPayload> payload = {};
    payload[0]                               = width;
    std::memcpy(payload.data() + 1, bitmap, bitmapLen);
    ESP_LOGI(TAG, "mBot send bitmap - width: %d - len: %d ", width, bitmapLen);
    return transactImpl(0x07, payload.data(), bitmapLen + 1, nullptr, 0);
}

bool MbotClient::clearDisplay()
{
    BusLockGuard guard(*this);
    if (!guard.locked() || !ensureLinkLocked()) {
        return false;
    }
    ESP_LOGI(TAG, "mBot clear display");
    return transactImpl(0x04, nullptr, 0, nullptr, 0);
}

bool MbotClient::transactImpl(uint8_t cmd, const uint8_t* payload, uint8_t payloadLen, uint8_t* responsePayload,
                              uint8_t expectedResponseLen)
{
    link_.lastCmd = cmd;

    if (!device_ || payloadLen > kMaxPayload) {
        markLinkDown(!device_ ? "no_device" : "payload_too_long");
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
    const uint8_t txTotal   = static_cast<uint8_t>(payloadLen + 4);
    request[3 + payloadLen] = crc8(request.data() + 1, payloadLen + 2);

    std::memcpy(link_.lastTxFrame.data(), request.data(), txTotal);
    link_.lastTxLen = txTotal;

    char hexTx[128];
    hexDumpToBuffer(link_.lastTxFrame.data(), link_.lastTxLen, hexTx, sizeof(hexTx));
    ESP_LOGI(TAG, "TX cmd=0x%02X payloadLen=%u (%u B frame): %s", cmd, payloadLen, txTotal, hexTx);

    esp_err_t txErr = i2c_master_transmit(device_, request.data(), txTotal, kTimeoutMs);
    if (txErr != ESP_OK) {
        link_.lastEspErr = txErr;
        markLinkDown("tx_failed");
        ESP_LOGW(TAG, "TX failed cmd=0x%02X esp_err=%s", cmd, esp_err_to_name(txErr));
        return false;
    }

    const int initialDelayMs = expectedResponseLen > 0 ? kReadCommandDelayMs : kInterFrameDelayMs;
    vTaskDelay(pdMS_TO_TICKS(initialDelayMs));

    const uint8_t expectedFrameLen = static_cast<uint8_t>(expectedResponseLen + 4);
    std::array<uint8_t, 32> response = {};
    char hexRx[128]                    = {};

    for (int attempt = 0; attempt < kRxMaxAttempts; ++attempt) {
        if (attempt > 0) {
            vTaskDelay(pdMS_TO_TICKS(kRxRetryDelayMs));
        }

        esp_err_t rxErr = i2c_master_receive(device_, response.data(), expectedFrameLen, kTimeoutMs);
        if (rxErr != ESP_OK) {
            link_.lastEspErr = rxErr;
            link_.lastRxLen  = 0;
            markLinkDown("rx_failed");
            ESP_LOGW(TAG, "RX failed cmd=0x%02X esp_err=%s (expected %u bytes)", cmd, esp_err_to_name(rxErr),
                     static_cast<unsigned>(expectedFrameLen));
            return false;
        }

        std::memcpy(link_.lastRxFrame.data(), response.data(), expectedFrameLen);
        link_.lastRxLen = expectedFrameLen;
        hexDumpToBuffer(link_.lastRxFrame.data(), link_.lastRxLen, hexRx, sizeof(hexRx));
        ESP_LOGV(TAG, "RX raw (try %d): %s", attempt + 1, hexRx);

        if (isStaleResponse(response.data(), cmd)) {
            ESP_LOGD(TAG, "stale RX for cmd=0x%02X (got cmd=0x%02X), retry %d/%d", cmd, response[2], attempt + 1,
                     kRxMaxAttempts);
            continue;
        }

        if (response[0] != kStartByte) {
            link_.lastBadByte = response[0];
            link_.lastReason  = "bad_start";
            link_.state       = LinkState::ProtocolError;
            if (!hasRecentTraffic()) {
                noteDisconnectedLocked();
            }
            ESP_LOGW(TAG, "bad START 0x%02X (want 0x%02X) cmd=0x%02X rx: %s", response[0], kStartByte, cmd, hexRx);
            return false;
        }

        const uint8_t wantLenField = static_cast<uint8_t>(expectedResponseLen + 2);
        if (response[1] != wantLenField) {
            link_.lastReason = "bad_len";
            link_.state      = LinkState::ProtocolError;
            if (!hasRecentTraffic()) {
                noteDisconnectedLocked();
            }
            ESP_LOGW(TAG, "bad LEN want %u got %u cmd=0x%02X rx: %s", wantLenField, response[1], cmd, hexRx);
            return false;
        }

        if (response[2] != cmd) {
            link_.lastReason = "bad_cmd";
            link_.state      = LinkState::ProtocolError;
            if (!hasRecentTraffic()) {
                noteDisconnectedLocked();
            }
            ESP_LOGW(TAG, "bad CMD echo want 0x%02X got 0x%02X rx: %s", cmd, response[2], hexRx);
            return false;
        }

        const uint8_t crcRx  = response[3 + expectedResponseLen];
        const uint8_t crcExp = crc8(response.data() + 1, static_cast<uint8_t>(expectedResponseLen + 2));
        if (crcExp != crcRx) {
            link_.lastReason = "bad_crc";
            link_.state      = LinkState::ProtocolError;
            if (!hasRecentTraffic()) {
                noteDisconnectedLocked();
            }
            ESP_LOGW(TAG, "bad CRC want 0x%02X got 0x%02X cmd=0x%02X rx: %s", crcExp, crcRx, cmd, hexRx);
            return false;
        }

        if (responsePayload && expectedResponseLen > 0) {
            std::memcpy(responsePayload, response.data() + 3, expectedResponseLen);
        }

        last_communication_ms_ = nowMs();
        ready_                 = true;
        link_.state            = LinkState::Ready;
        link_.lastEspErr       = ESP_OK;
        link_.lastReason       = "";
        link_down_since_ms_    = 0;
        return true;
    }

    link_.lastReason = "stale_response";
    link_.state      = LinkState::ProtocolError;
    if (!hasRecentTraffic()) {
        noteDisconnectedLocked();
    }
    ESP_LOGW(TAG, "no matching RX for cmd=0x%02X after %d attempts (last: %s)", cmd, kRxMaxAttempts, hexRx);
    return false;
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

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <esp_err.h>
#include <hal/drivers/mbot/mbot_client.h>

#include <cstdio>
#include <string>

namespace view {

inline std::string formatMbotLinkMessage(const MbotClient::LinkStatus& s, const char* exitHomeHint = "\n\nSwipe up: Home")
{
    char errbuf[48];
    errbuf[0] = '\0';
    if (s.lastEspErr != ESP_OK) {
        snprintf(errbuf, sizeof(errbuf), "%s", esp_err_to_name(s.lastEspErr));
    }

    switch (s.state) {
        case MbotClient::LinkState::Probing:
            return std::string("Probing mBot at 0x10\nattempt #") + std::to_string(s.attempt) + exitHomeHint;
        case MbotClient::LinkState::ProbeNack:
            return std::string("mBot not detected at 0x10\n(NACK / not on bus)\n\nesp_err: ") + errbuf +
                   "\n\n"
                   "Check:\n"
                   " - mBot powered ON\n"
                   " - mBot firmware running\n"
                   " - RJ25 Port 1 SDA/SCL\n"
                   " - Power mBot before StackChan" + std::string(exitHomeHint);
        case MbotClient::LinkState::ProbeTimeout:
            return std::string("mBot probe TIMEOUT\n(bus may be stuck)\n\nesp_err: ") + errbuf +
                   "\n\n"
                   "Check SDA/SCL pull-ups / wiring\n"
                   "Also verify mBot ON + Port 1." + std::string(exitHomeHint);
        case MbotClient::LinkState::AddDeviceFailed:
            return std::string("I2C add_device failed\nesp_err: ") + errbuf + exitHomeHint;
        case MbotClient::LinkState::Handshaking:
            return std::string("mBot on bus\nWaiting for PING ACK...\nattempt #") + std::to_string(s.attempt) +
                   exitHomeHint;
        case MbotClient::LinkState::ProtocolError: {
            std::string msg = "Protocol error: ";
            msg += (s.lastReason && s.lastReason[0]) ? s.lastReason : "?";
            char hx[16];
            snprintf(hx, sizeof(hx), "\nlast cmd 0x%02X", s.lastCmd);
            msg += hx;
            if (s.lastEspErr != ESP_OK) {
                msg += "\nesp_err: ";
                msg += esp_err_to_name(s.lastEspErr);
            }
            msg += exitHomeHint;
            return msg;
        }
        case MbotClient::LinkState::Ready:
            return "mBot connected";
        case MbotClient::LinkState::Idle:
        default:
            return "Starting mBot I2C ...\nLong-press: bus scan\nSwipe up: Home";
    }
}

/** Compact one-liner for red error toasts (Avatar app style). */
inline std::string formatMbotErrorToast(const MbotClient::LinkStatus& s)
{
    switch (s.state) {
        case MbotClient::LinkState::ProtocolError: {
            std::string msg = "mBot error: ";
            msg += (s.lastReason && s.lastReason[0]) ? s.lastReason : "protocol";
            return msg;
        }
        case MbotClient::LinkState::ProbeNack:
            return "mBot not detected on I2C bus";
        case MbotClient::LinkState::ProbeTimeout:
            return "mBot I2C timeout";
        case MbotClient::LinkState::AddDeviceFailed:
            return "mBot I2C setup failed";
        case MbotClient::LinkState::Handshaking:
            return "mBot handshake failed";
        case MbotClient::LinkState::Probing:
            return "mBot probe failed";
        default:
            return "mBot connection error";
    }
}

}  // namespace view

/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_ai_agent_body.h"

#include <apps/common/home_indicator/home_indicator.h>
#include <apps/common/loading_page/loading_page.h>
#include <assets/assets.h>
#include <esp_err.h>
#include <hal/drivers/mbot/mbot_client.h>
#include <hal/hal.h>
#include <mooncake_log.h>

#include <algorithm>
#include <cstdio>
#include <string>

using namespace mooncake;

namespace {

constexpr const char* kExitHomeHint = "\n\nSwipe up: Home";

std::string formatMbotLinkMessage(const MbotClient::LinkStatus& s)
{
    char errbuf[48];
    errbuf[0] = '\0';
    if (s.lastEspErr != ESP_OK) {
        snprintf(errbuf, sizeof(errbuf), "%s", esp_err_to_name(s.lastEspErr));
    }

    switch (s.state) {
        case MbotClient::LinkState::Probing:
            return std::string("Probing mBot at 0x10\nattempt #") + std::to_string(s.attempt) + kExitHomeHint;
        case MbotClient::LinkState::ProbeNack:
            return std::string("mBot not detected at 0x10\n(NACK / not on bus)\n\nesp_err: ") + errbuf +
                   "\n\n"
                   "Check:\n"
                   " - mBot powered ON\n"
                   " - mBot firmware running\n"
                   "   (matrix: BOOT / I2C?)\n"
                   " - RJ25 Port 1 SDA/SCL\n"
                   " - Power mBot before StackChan" + std::string(kExitHomeHint);
        case MbotClient::LinkState::ProbeTimeout:
            return std::string("mBot probe TIMEOUT\n(bus may be stuck)\n\nesp_err: ") + errbuf +
                   "\n\n"
                   "Check SDA/SCL pull-ups / wiring\n"
                   "Also verify mBot ON + Port 1." + std::string(kExitHomeHint);
        case MbotClient::LinkState::AddDeviceFailed:
            return std::string("I2C add_device failed\nesp_err: ") + errbuf + kExitHomeHint;
        case MbotClient::LinkState::Handshaking:
            return std::string("mBot on bus\nWaiting for PING ACK...\nattempt #") + std::to_string(s.attempt) +
                   kExitHomeHint;
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
            msg += kExitHomeHint;
            return msg;
        }
        case MbotClient::LinkState::Ready:
            return "mBot connected";
        case MbotClient::LinkState::Idle:
        default:
            return "Starting mBot I2C ...\nLong-press: bus scan\nSwipe up: Home button";
    }
}

}  // namespace

AppAiAgentBody::AppAiAgentBody()
{
    setAppInfo().name = "AI.BODY";
    static auto icon  = assets::get_image("icon_ai_agent.bin");
    setAppInfo().icon = (void*)&icon;
    static uint32_t theme_color = 0x33CC99;
    setAppInfo().userData       = (void*)&theme_color;
}

AppAiAgentBody::~AppAiAgentBody() = default;

void AppAiAgentBody::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppAiAgentBody::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");
    GetHAL().setMbotBodyMotionEnabled(true);

    MbotClient::GetInstance().resetConnectionDiagnostics();

    {
        LvglLockGuard lock;

        loading_page_            = std::make_unique<view::LoadingPage>(0x000000, 0xFFFFFF);
        next_connect_attempt_ms_ = 0;
        connected_at_ms_         = 0;
        connect_backoff_ms_      = 500;
        mbot_ok_sent_            = false;

        loading_page_->setMessage(formatMbotLinkMessage(MbotClient::GetInstance().snapshot()));
        loading_page_->setOnLongPress([this]() {
            std::string scan = MbotClient::rescanBus();
            loading_page_->setMessage(std::string("I2C scan (long-press to refresh):\n") + scan);
        });

        // Same pattern as EZDATA / DANCE: swipe up reveals chip, tap returns to launcher.
        view::create_home_indicator([&]() { close(); }, 0x33CC99, 0x0D4D3D);
    }
}

void AppAiAgentBody::onRunning()
{
    LvglLockGuard lock;

    auto now   = GetHAL().millis();
    auto& mbot = MbotClient::GetInstance();

    if (!connected_at_ms_ && now >= next_connect_attempt_ms_) {
        const bool ok = mbot.tryConnectOnce();
        if (ok) {
            connected_at_ms_    = now;
            connect_backoff_ms_ = 500;
            loading_page_->setMessage("mBot connected");
            if (!mbot_ok_sent_) {
                mbot.displayText("OK");
                mbot_ok_sent_ = true;
            }
        } else {
            loading_page_->setMessage(formatMbotLinkMessage(mbot.snapshot()));
            next_connect_attempt_ms_ = now + connect_backoff_ms_;
            connect_backoff_ms_      = std::min<uint32_t>(connect_backoff_ms_ * 2U, 5000U);
        }
    }

    if (connected_at_ms_ && now - connected_at_ms_ >= 500) {
        GetHAL().requestXiaozhiStart();
    }

    view::update_home_indicator();
}

void AppAiAgentBody::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    {
        LvglLockGuard lock;

        loading_page_.reset();
        view::destroy_home_indicator();
    }

    GetHAL().setMbotBodyMotionEnabled(false);

    /* Scroll launcher carousel back to AI.BODY slot (app install order in main.cpp). */
    GetHAL().requestWarmReboot(2);
}

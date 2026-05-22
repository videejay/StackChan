/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_web_remote.h"

#include <apps/common/home_indicator/home_indicator.h>
#include <apps/common/mbot/mbot_link_message.h>
#include <apps/common/status_bar/status_bar.h>
#include <assets/assets.h>
#include <hal/drivers/mbot/mbot_client.h>
#include <hal/hal.h>
#include <hal/mbot_web/mbot_web_server.h>
#include <mooncake_log.h>
#include <stackchan/avatar/avatar_factory.h>
#include <stackchan/modifiers/blink.h>
#include <stackchan/modifiers/breath.h>
#include <stackchan/stackchan.h>

#include <algorithm>
#include <cstdio>
#include <string>

using namespace mooncake;
using namespace stackchan;

AppWebRemote::AppWebRemote()
{
    setAppInfo().name = "WEB.REMOTE";
    static auto icon  = assets::get_image("icon_ai_robot_remote.bin");
    setAppInfo().icon = (void*)&icon;
    static uint32_t theme_color = kThemeColor;
    setAppInfo().userData       = (void*)&theme_color;
}

void AppWebRemote::onCreate()
{
    mclog::tagInfo(getAppInfo().name, "on create");
}

void AppWebRemote::formatUrlMessage(std::string& out) const
{
    const auto ip = GetHAL().getWifiIpAddress();
    char buf[192];
    snprintf(buf, sizeof(buf), "Open in browser:\n\nhttp://%s\n\nPort 80", ip.empty() ? "?" : ip.c_str());
    out = buf;
}

void AppWebRemote::showAvatar()
{
    if (avatar_attached_) {
        return;
    }

    auto& stackchan = GetStackChan();
    stackchan.attachAvatar(avatar::createConfiguredAvatar(lv_screen_active()));
    stackchan.clearModifiers();
    stackchan.addModifier(std::make_unique<BreathModifier>());
    stackchan.addModifier(std::make_unique<BlinkModifier>());
    stackchan.motion().setAutoAngleSyncEnabled(false);
    avatar_attached_ = true;

    view::create_status_bar(kThemeColor, kThemeColorDark);
}

void AppWebRemote::onOpen()
{
    mclog::tagInfo(getAppInfo().name, "on open");

    MbotClient::GetInstance().resetConnectionDiagnostics();

    phase_                    = Phase::EnsureWifi;
    network_start_attempted_  = false;
    avatar_attached_          = false;
    next_connect_attempt_ms_  = 0;
    connect_backoff_ms_       = 500;
    url_shown_at_ms_          = 0;
    last_keepalive_ms_        = 0;

    LvglLockGuard lock;
    loading_page_ = std::make_unique<view::LoadingPage>(0x000000, 0xFFFFFF);
    loading_page_->setMessage("Checking WiFi...");
    loading_page_->setOnLongPress([this]() {
        std::string scan = MbotClient::rescanBus();
        loading_page_->setMessage(std::string("I2C scan (long-press to refresh):\n") + scan);
    });
    view::create_home_indicator([&]() { close(); }, kThemeColor, kThemeColorDark);
}

void AppWebRemote::serviceMbotKeepalive(uint32_t now)
{
    if (phase_ != Phase::ShowUrl && phase_ != Phase::AvatarIdle) {
        return;
    }

    auto& mbot = MbotClient::GetInstance();
    if (now - last_keepalive_ms_ >= kKeepaliveMs) {
        mbot.serviceKeepAlive();
        last_keepalive_ms_ = now;
        return;
    }

    // Between ping intervals still verify link (cheap; does not ping).
    mbot.maintainLink();
}

void AppWebRemote::onRunning()
{
    const auto now = GetHAL().millis();

    // Highest priority: mBot link must stay up while using avatar/camera (no I2C traffic otherwise).
    serviceMbotKeepalive(now);

    if (mbotWebIsActive()) {
        mbotWebAvatarTick();
    }

    LvglLockGuard lock;

    auto& mbot = MbotClient::GetInstance();

    switch (phase_) {
        case Phase::EnsureWifi:
            if (GetHAL().isWifiConnected()) {
                loading_page_->setMessage("Connecting mBot...");
                phase_ = Phase::ConnectMbot;
                break;
            }
            if (!network_start_attempted_) {
                network_start_attempted_ = true;
                loading_page_->setMessage("Connecting WiFi...");
                GetHAL().lvglUnlock();
                GetHAL().startNetwork([&](std::string_view msg) {
                    LvglLockGuard inner;
                    if (loading_page_) {
                        loading_page_->setMessage(msg);
                    }
                });
                GetHAL().lvglLock();
                if (GetHAL().isWifiConnected()) {
                    loading_page_->setMessage("Connecting mBot...");
                    phase_ = Phase::ConnectMbot;
                } else {
                    loading_page_->setMessage("Configure WiFi in Setup\n\nSwipe up: Home");
                    phase_ = Phase::Error;
                }
            } else {
                loading_page_->setMessage("Configure WiFi in Setup\n\nSwipe up: Home");
                phase_ = Phase::Error;
            }
            break;

        case Phase::ConnectMbot:
            if (now >= next_connect_attempt_ms_) {
                if (mbot.maintainLink()) {
                    phase_ = Phase::StartHttp;
                } else {
                    loading_page_->setMessage(view::formatMbotLinkMessage(mbot.snapshot()));
                    next_connect_attempt_ms_ = now + connect_backoff_ms_;
                    connect_backoff_ms_ =
                        std::min<uint32_t>(connect_backoff_ms_ * 2U, 5000U);
                }
            }
            break;

        case Phase::StartHttp:
            if (mbotWebStart(80)) {
                std::string msg;
                formatUrlMessage(msg);
                loading_page_->setMessage(msg);
                url_shown_at_ms_ = now;
                last_keepalive_ms_ = now;
                phase_           = Phase::ShowUrl;
            } else {
                loading_page_->setMessage("HTTP server failed\n\nSwipe up: Home");
                phase_ = Phase::Error;
            }
            break;

        case Phase::ShowUrl:
            if (!mbot.maintainLink()) {
                loading_page_->setMessage(view::formatMbotLinkMessage(mbot.snapshot()));
                phase_                   = Phase::ConnectMbot;
                next_connect_attempt_ms_ = now;
                connect_backoff_ms_      = 500;
                break;
            }
            if (now - url_shown_at_ms_ >= kUrlDisplayMs) {
                loading_page_.reset();
                showAvatar();
                phase_ = Phase::AvatarIdle;
            }
            break;

        case Phase::AvatarIdle:
            if (!mbot.maintainLink()) {
                avatar_attached_ = false;
                GetStackChan().resetAvatar();
                view::destroy_status_bar();
                loading_page_ = std::make_unique<view::LoadingPage>(0x000000, 0xFFFFFF);
                loading_page_->setMessage(view::formatMbotLinkMessage(mbot.snapshot()));
                loading_page_->setOnLongPress([this]() {
                    std::string scan = MbotClient::rescanBus();
                    loading_page_->setMessage(std::string("I2C scan (long-press to refresh):\n") + scan);
                });
                phase_                   = Phase::ConnectMbot;
                next_connect_attempt_ms_ = now;
                connect_backoff_ms_      = 500;
                break;
            }
            GetStackChan().update();
            view::update_status_bar();
            view::update_home_indicator();
            break;

        case Phase::Error:
        default:
            view::update_home_indicator();
            break;
    }
}

void AppWebRemote::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");

    mbotWebStop();

    LvglLockGuard lock;
    loading_page_.reset();
    if (avatar_attached_) {
        GetStackChan().resetAvatar();
        avatar_attached_ = false;
    }
    if (view::is_status_bar_created()) {
        view::destroy_status_bar();
    }
    view::destroy_home_indicator();

    /* Launcher carousel index for WEB.REMOTE (install order in main.cpp). */
    GetHAL().requestWarmReboot(5);
}

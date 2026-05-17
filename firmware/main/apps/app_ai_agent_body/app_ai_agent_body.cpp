/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "app_ai_agent_body.h"

#include <apps/common/loading_page/loading_page.h>
#include <assets/assets.h>
#include <hal/hal.h>
#include <hal/drivers/mbot/mbot_client.h>
#include <mooncake_log.h>

using namespace mooncake;

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

    loading_page_ = std::make_unique<view::LoadingPage>(0x000000, 0xFFFFFF);
    loading_page_->setMessage("Connecting to mBot\nvia I2C ...");
    next_connect_attempt_ms_ = 0;
    connected_at_ms_         = 0;
}

void AppAiAgentBody::onRunning()
{
    auto now   = GetHAL().millis();
    auto& mbot = MbotClient::GetInstance();

    if (!connected_at_ms_ && now >= next_connect_attempt_ms_) {
        next_connect_attempt_ms_ = now + 500;
        if (mbot.init()) {
            loading_page_->setMessage("mBot connected");
            mbot.displayText("OK");
            connected_at_ms_ = now;
        }
    }

    if (connected_at_ms_ && now - connected_at_ms_ >= 500) {
        GetHAL().requestXiaozhiStart();
    }
}

void AppAiAgentBody::onClose()
{
    mclog::tagInfo(getAppInfo().name, "on close");
}
